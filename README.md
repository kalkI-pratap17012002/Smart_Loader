# SmartLoader

> A demand-paged ELF binary loader in C — intercepts SIGSEGV to implement true lazy loading, allocating exactly one 4KB page per fault rather than mapping entire segments upfront.

---

## Overview

A naive ELF loader reads all program segments into memory before jumping to the entry point. SmartLoader does the opposite: it calls the entry point immediately with nothing mapped. When the program accesses any address, the OS raises `SIGSEGV`. SmartLoader's signal handler catches it, identifies which 4KB page the faulting address belongs to, maps exactly that page with `mmap(MAP_FIXED)`, copies the relevant segment bytes from the ELF file, and returns — the faulting instruction retries and succeeds. The process repeats on every new page access until the program exits.

```
Entry point called (no memory mapped)
        │
        ▼ SIGSEGV
┌──────────────────────────────┐
│      handle_segfault()       │
│  1. compute faulting page    │
│  2. check already mapped?    │
│  3. find containing segment  │
│  4. mmap(MAP_FIXED, 4KB)     │
│  5. copy segment bytes in    │
│  6. return → retry faulting  │
│     instruction              │
└──────────────────────────────┘
        │
        ▼ (repeat per new page)
   Program runs to completion

Total Page Faults:       12
Total Page Allocations:  12
Internal Fragmentation:  2176 Bytes (2.13 KB)
```

---

## Key Features

- **True lazy loading** — no segment is mapped until a byte within it is actually accessed
- **Out-of-order page access** — if execution jumps to page 5 before page 1, only page 5 is mapped; page 1 is never touched
- **Exact 4KB allocation per fault** — never maps more than one page per SIGSEGV
- **Double-map prevention** — linked list of allocated pages checked on every fault
- **Accurate fragmentation reporting** — internal fragmentation computed only on the final page of each segment
- **Complete cleanup** — `munmap` called for every allocated page on exit; no leaks
- **32-bit ELF support** — targets `-m32 -no-pie -nostdlib` binaries; validates ELF magic and class before loading

---

## How It Works

### 1. ELF Parsing

`load_and_execute_elf()` opens the binary, validates the ELF header (magic bytes, 32-bit class, executable type), reads all program headers into memory, and jumps directly to `e_entry` — the program's entry point — without mapping any segment.

### 2. Signal Handler Registration

Before calling the entry point, `sigaction` installs `handle_segmentation_fault()` as the `SIGSEGV` handler with `SA_SIGINFO` so the handler receives the faulting address via `siginfo_t`.

### 3. Page Fault Handling

```
fault_addr  = si->si_addr
page_addr   = fault_addr aligned down to 4KB boundary

if is_page_mapped(page_addr): genuine SIGSEGV → re-raise, abort
find segment S such that page_addr falls within S's virtual range
mmap(page_addr, 4096, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_FIXED|MAP_PRIVATE, -1, 0)
copy bytes from ELF file offset into mapped page
record page_addr in allocated_page linked list
```

### 4. Fragmentation Calculation

Internal fragmentation exists only on the last page of a segment (preceding pages are fully used). It is computed as:

```
fragmentation = PAGE_SIZE - (segment_filesz % PAGE_SIZE)
```

Counted only when the final page of a segment is allocated.

---

## Build & Run

**Requirements:** GCC with `-m32` support (`gcc-multilib`), GNU Make, Linux x86_64

```bash
make              # builds loader + all test binaries
make clean        # removes all compiled outputs
```

```bash
./loader ./fib_40
./loader ./fib_10
./loader ./sum
```

### Expected Output

```
Starting ELF execution...
Program returned: 102334155

Execution Statistics:
Total Page Faults:       12
Total Page Allocations:  12
Internal Fragmentation:  2176 Bytes (2.13 KB)
```

---

## Implementation Details

### Page Tracking — Linked List

```c
typedef struct allocated_page {
    void *addr;
    struct allocated_page *next;
} allocated_page;
```

A singly-linked list records every mapped page address. On each fault, `is_page_mapped()` walks the list — O(n) but negligible for typical page counts. On exit, `cleanup_loader()` calls `munmap` for every node and frees the list.

### `mmap` Parameters

```c
mmap(page_addr, PAGE_SIZE,
     PROT_READ | PROT_WRITE | PROT_EXEC,
     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
     -1, 0);
```

`MAP_FIXED` places the mapping at exactly the required address. `MAP_ANONYMOUS` with `fd=-1` allocates a zeroed page; segment bytes are then copied in from the ELF file using the program header's `p_offset` and `p_filesz`.

### ELF Validation

Before loading, the header is checked for:
- Magic bytes: `0x7f 'E' 'L' 'F'`
- `e_ident[EI_CLASS] == ELFCLASS32`
- `e_type == ET_EXEC`

Invalid binaries are rejected with a clear error message before any memory is mapped.

---

## Test Cases

| Binary | Description | Page Faults (approx) |
|---|---|---|
| `fib_40` | Recursive Fibonacci(40) — deep stack | high |
| `fib_35` – `fib_10` | Progressively smaller recursion depth | decreasing |
| `sum` | Array summation — data-segment heavy | moderate |

All test binaries are compiled with `-m32 -no-pie -nostdlib` and use only raw Linux syscalls.

---

## Project Structure

```
.
├── loader.c          # SmartLoader implementation (229 lines)
├── fib_10.c – fib_40.c  # Recursive Fibonacci test cases
├── sum.c             # Array summation test case
└── Makefile
```

---

## Known Limitations

- 32-bit ELF only (`ELFCLASS32`) — 64-bit binaries are rejected at validation
- Test programs must be compiled with `-nostdlib` (no glibc dependency)
- Linux only — relies on `mmap`, `sigaction`, and Linux signal semantics
- Page tracking is O(n) linked list scan — suitable for typical page counts, not thousands

---

## Tech Stack

`C` · `ELF Binary Format` · `SIGSEGV / sigaction` · `mmap(MAP_FIXED)` · `Demand Paging` · `32-bit x86` · `Linux`

---

## Repository

[github.com/kalkI-pratap17012002/Smart_Loader](https://github.com/kalkI-pratap17012002/Smart_Loader.git)
