# SmartLoader — Technical Design Document

**Project:** Demand-paged ELF loader using signal-based page fault handling  
**Author:** Ayush Kumar  
**Stack:** C · ELF · SIGSEGV · mmap(MAP_FIXED) · sigaction · 32-bit x86 · Linux

---

## Table of Contents

1. [Problem Statement](#1-problem-statement)
2. [Design Goals](#2-design-goals)
3. [Architecture](#3-architecture)
4. [ELF Loading Strategy](#4-elf-loading-strategy)
5. [Signal-Based Page Fault Handling](#5-signal-based-page-fault-handling)
6. [Page Allocation with mmap(MAP_FIXED)](#6-page-allocation-with-mmapmap_fixed)
7. [Double-Map Prevention](#7-double-map-prevention)
8. [Fragmentation Calculation](#8-fragmentation-calculation)
9. [Memory Cleanup](#9-memory-cleanup)
10. [Design Trade-offs](#10-design-trade-offs)
11. [Testing Strategy](#11-testing-strategy)
12. [Known Limitations & Future Work](#12-known-limitations--future-work)

---

## 1. Problem Statement

A standard ELF loader (like a simplified `execve`) reads all `PT_LOAD` segments from the binary into memory before transferring control to the entry point. This is simple but wasteful: a program that uses 10% of its code segment still pays the cost of mapping 100% of it upfront.

The OS kernel solves this with demand paging — backed by `SIGSEGV` and page table entries marked not-present. SmartLoader replicates this mechanism entirely in user space:

- No kernel modifications
- No pre-allocation of any segment
- Every memory page is mapped exactly when — and only when — a byte within it is first accessed

The core challenge is that `SIGSEGV` is normally fatal. The design must distinguish between a recoverable page fault (an address within a known ELF segment, not yet mapped) and a genuine segmentation fault (an invalid access that should terminate the program).

---

## 2. Design Goals

| Priority | Goal | Rationale |
|---|---|---|
| P0 | **True laziness** | No byte of any segment is mapped before it is accessed |
| P0 | **Exactly 4KB per fault** | Never map more than the faulting page, even if the segment is larger |
| P0 | **Correct fault discrimination** | Genuine SIGSEGV must still terminate the program; only valid ELF faults are handled |
| P1 | **Out-of-order access support** | Page 5 can be mapped before page 1 — access pattern is not assumed |
| P1 | **Accurate statistics** | Page fault count, allocation count, and fragmentation must be correct |
| P2 | **Complete cleanup** | Every mapped page must be unmapped on exit |
| P2 | **ELF validation** | Reject non-ELF, 64-bit, and non-executable binaries before any mapping |

---

## 3. Architecture

```
main()
  │
  ├── load_and_execute_elf(path)
  │       │
  │       ├── open ELF file
  │       ├── validate header (magic, ELFCLASS32, ET_EXEC)
  │       ├── read program headers into memory
  │       ├── install SIGSEGV handler (sigaction + SA_SIGINFO)
  │       └── call entry_point()   ← no segments mapped yet
  │
  │   ┌─────────────────────────────────────────────────────┐
  │   │  Program executes — any unmapped access → SIGSEGV   │
  │   └─────────────────────────────────────────────────────┘
  │
  └── handle_segmentation_fault(sig, si, ctx)
          │
          ├── fault_addr = si->si_addr
          ├── page_addr  = align_down(fault_addr, PAGE_SIZE)
          │
          ├── is_page_mapped(page_addr)?
          │       YES → genuine SIGSEGV → signal(SIGSEGV, SIG_DFL) + raise
          │
          ├── find segment: p_vaddr ≤ fault_addr < p_vaddr + p_memsz
          │       not found → genuine SIGSEGV → abort
          │
          ├── mmap(page_addr, 4096, RWX, MAP_FIXED|MAP_ANON, -1, 0)
          ├── copy bytes from ELF file into mapped page
          ├── push page_addr onto allocated_page list
          │
          └── return → faulting instruction retries → succeeds

  cleanup_loader()
          │
          └── munmap each node in allocated_page list → free list
```

---

## 4. ELF Loading Strategy

### Why Not Map Upfront

A conventional loader iterates `PT_LOAD` headers and calls `mmap` for each segment before jumping to `e_entry`. SmartLoader intentionally skips this entirely. The program headers are read and kept in memory (so the fault handler can look up segments), but no virtual address range is mapped.

### Entry Point Call

```c
int (*entry_point)() = (int(*)()) elf_header.e_entry;
int result = entry_point();
```

The entry point is called with no mapped memory. The first instruction access triggers a `SIGSEGV` immediately — which is the intended behavior.

### ELF Validation

Before proceeding, the header is checked:

```c
// Magic
if (memcmp(hdr.e_ident, "\x7fELF", 4) != 0)  → error

// 32-bit class
if (hdr.e_ident[EI_CLASS] != ELFCLASS32)       → error

// Executable type
if (hdr.e_type != ET_EXEC)                     → error
```

This prevents undefined behavior from attempting to "load" a shared library, a 64-bit binary, or a non-ELF file.

---

## 5. Signal-Based Page Fault Handling

### Signal Installation

```c
struct sigaction sa;
sa.sa_sigaction = handle_segmentation_fault;
sa.sa_flags     = SA_SIGINFO;   // receive siginfo_t with fault address
sigemptyset(&sa.sa_mask);
sigaction(SIGSEGV, &sa, NULL);
```

`SA_SIGINFO` is critical — without it the handler receives only the signal number, not the faulting address. `si->si_addr` provides the exact byte that caused the fault.

### Fault Discrimination

Not every `SIGSEGV` is a page fault we should handle. Two cases require passing the signal through as a genuine fault:

1. **Already-mapped address** — if `is_page_mapped(page_addr)` returns true, the page exists but the access is still invalid (e.g. a write to a read-only page, or a NULL dereference that happens to hit a mapped region). Re-raise with default handler.

2. **Address outside all ELF segments** — if no `PT_LOAD` segment contains `fault_addr`, it is a genuine invalid access. Abort.

```c
// restore default and re-raise → program terminates with SIGSEGV as expected
signal(SIGSEGV, SIG_DFL);
raise(SIGSEGV);
```

This is what makes the fault handler safe — it does not silently swallow every SIGSEGV.

---

## 6. Page Allocation with mmap(MAP_FIXED)

### Page Address Calculation

```c
void *page_addr = (void *)((uintptr_t)fault_addr & ~(PAGE_SIZE - 1));
```

Bitwise AND with the inverted page mask aligns down to the 4KB boundary containing the faulting address.

### mmap Call

```c
void *mapped = mmap(
    page_addr,
    PAGE_SIZE,
    PROT_READ | PROT_WRITE | PROT_EXEC,
    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
    -1, 0
);
```

`MAP_FIXED` places the mapping at exactly `page_addr`. Without it, the kernel may choose a different address. `MAP_ANONYMOUS` with `fd=-1` allocates a zeroed page — segment bytes are then written in explicitly.

### Copying Segment Data

The portion of the ELF segment that falls within the mapped page is computed from the program header and copied from the file:

```c
// offset into segment where this page begins
size_t seg_offset = page_addr - (void*)phdr->p_vaddr;

// how many bytes from the file to copy (capped at page boundary and filesz)
size_t copy_size = min(PAGE_SIZE, phdr->p_filesz - seg_offset);

lseek(elf_fd, phdr->p_offset + seg_offset, SEEK_SET);
read(elf_fd, page_addr, copy_size);
```

Pages beyond `p_filesz` (BSS region) are left zeroed — correct behavior since `MAP_ANONYMOUS` zero-initializes.

---

## 7. Double-Map Prevention

If the same page is faulted twice (e.g. due to a re-entrant signal or a bug), attempting `mmap(MAP_FIXED)` on an already-mapped address would silently replace the existing mapping, corrupting program state.

The `is_page_mapped()` function walks the `allocated_page` linked list before every `mmap` call:

```c
int is_page_mapped(void *page_addr) {
    allocated_page *curr = page_list_head;
    while (curr) {
        if (curr->addr == page_addr) return 1;
        curr = curr->next;
    }
    return 0;
}
```

If the page is already in the list, the handler treats the fault as genuine and re-raises rather than remapping.

---

## 8. Fragmentation Calculation

### What Is Being Measured

Internal fragmentation in a page-allocated system is the wasted space in the last page of a segment. If a segment's last byte lands at offset 2048 within a 4096-byte page, 2048 bytes of that page are unused.

### Formula

```
fragmentation = PAGE_SIZE - (segment_filesz % PAGE_SIZE)
```

Only applied when `segment_filesz % PAGE_SIZE != 0` (a full final page has zero fragmentation). Only counted when the last page of a given segment is actually allocated — if a program never accesses the tail of a segment, no fragmentation is recorded.

### Per-Segment Tracking

Each program header is checked: when the page being allocated is the final page of that segment, the fragmentation for that segment is added to the running total.

---

## 9. Memory Cleanup

`cleanup_loader()` is called after the entry point returns:

```c
void cleanup_loader() {
    allocated_page *curr = page_list_head;
    while (curr) {
        munmap(curr->addr, PAGE_SIZE);
        allocated_page *next = curr->next;
        free(curr);
        curr = next;
    }
    page_list_head = NULL;
    free(program_headers);
    close(elf_fd);
}
```

Every node in the linked list is unmapped and freed. The program headers buffer and file descriptor are also released. After `cleanup_loader()` returns, the process holds no lingering resources.

---

## 10. Design Trade-offs

| Decision | Chosen | Rejected | Reason |
|---|---|---|---|
| Page tracking | Linked list | Bitmap / hash table | Simple; sufficient for typical page counts; easy O(1) insert |
| Segment lookup | Linear scan of program headers | Interval tree | Number of PT_LOAD segments is small (2–4 typically); O(n) is negligible |
| mmap flags | MAP_ANONYMOUS + MAP_FIXED | mmap the ELF file directly | Allows BSS zero-fill naturally; avoids file offset alignment constraints |
| Data copy | read() after lseek() | pread() | Functionally identical; lseek+read is more portable |
| Fault discrimination | Linked list check + segment check | /proc/self/maps parse | No subprocess required; faster; no external dependency |
| Fragmentation scope | Last page per segment only | All pages | Accurately models internal fragmentation — only the last page can be partially used |
| Cleanup trigger | After entry_point() returns | atexit() handler | Explicit control; atexit unreliable in nostdlib context |

---

## 11. Testing Strategy

### Test Binaries

| Binary | Nature | What it exercises |
|---|---|---|
| `fib_10` – `fib_40` | Recursive Fibonacci | Deep call stacks — stack segment grows page by page |
| `sum` | Array summation | Sequential data access — data segment faults in order |

All test binaries compiled with:
```bash
gcc -m32 -no-pie -nostdlib -o fib_40 fib_40.c
```

No `glibc` — programs use only raw Linux syscalls via `int 0x80`.

### Correctness Checks

- Return value matches known Fibonacci results (`fib(40) = 102334155`)
- Page fault count is plausible for the binary size and recursion depth
- Fragmentation is non-zero and bounded below `PAGE_SIZE` per segment
- Running the same binary twice produces identical statistics

### Edge Case Tests

| Case | Expected Behavior |
|---|---|
| Double fault on same page | Detected by `is_page_mapped()`; genuine SIGSEGV raised |
| Non-ELF file passed as argument | Rejected at ELF magic validation |
| 64-bit ELF passed | Rejected at `ELFCLASS32` check |
| Segment with `filesz < memsz` (BSS) | BSS pages zeroed by `MAP_ANONYMOUS`; no garbage data |
| Program that never accesses segment tail | No fragmentation recorded for that segment |

---

## 12. Known Limitations & Future Work

**Current Limitations**

- 32-bit ELF only — `ELFCLASS64` binaries are unsupported
- `nostdlib` requirement — programs with glibc dependencies cannot be loaded (dynamic linking not implemented)
- Linux only — `mmap`, `sigaction`, and `SA_SIGINFO` semantics are Linux-specific
- O(n) page lookup — not a bottleneck for typical programs, but would not scale to thousands of pages

**Future Improvements**

- **64-bit support** — use `Elf64_Ehdr` and `Elf64_Phdr`; handle 64-bit address arithmetic
- **Dynamic linking** — parse `.dynamic` section, load shared libraries, resolve PLT entries
- **Hash-based page table** — replace linked list with hash map for O(1) `is_page_mapped()` lookup
- **Copy-on-write semantics** — map read-only segments as `PROT_READ` initially; promote to writable only on write fault
- **Integration with EGOS-2000** — the loader was extended into a kernel-level ELF loader as part of EGOS-2000 kernel modifications

---

*SmartLoader demonstrates that the OS kernel's demand-paging mechanism can be faithfully replicated in user space — using SIGSEGV not as an error, but as a notification, and mmap(MAP_FIXED) as a precise page table manipulation primitive.*
