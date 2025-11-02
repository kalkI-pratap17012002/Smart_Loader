# SimpleSmartLoader - 2025 Assignment 4

## Overview

SimpleSmartLoader is an upgraded version of SimpleLoader that implements lazy, page-by-page loading of ELF 32-bit executables. It uses segmentation fault handling to allocate memory on-demand rather than loading all segments upfront.

## Features

- **Lazy Loading**: Segments are loaded only when accessed
- **Page-by-Page Allocation**: Memory is allocated in 4KB pages as needed
- **Segmentation Fault Handling**: Converts SIGSEGV into page fault handling
- **Statistics Reporting**: Tracks page faults, allocations, and fragmentation



## Building the Project

On your Linux/WSL system:

```bash
make
```

This will compile:
- `loader` - The SimpleSmartLoader executable
- `fib_40` - Fibonacci test case
- `fib_35` - Fibonacci test case
- `fib_30` - Fibonacci test case
- `fib_25` - Fibonacci test case
- `fib_20` - Fibonacci test case
- `fib_15` - Fibonacci test case
- `fib_10` - Fibonacci test case
- `sum` - Array summation test case

## Running the Loader

```bash
./loader ./fib_40
./loader ./sum
```

## Expected Output

```
Starting ELF execution...
Program returned: <return value>

Execution Statistics:
Total Page Faults: <count>
Total Page Allocations: <count>
Internal Fragmentation: <bytes> Bytes (<KB> KB)
```

## How It Works

1. **Entry Point Call**: Loader directly calls the ELF entry point without pre-loading any segments
2. **First Page Fault**: When execution attempts to access unmapped memory, SIGSEGV is triggered
3. **Signal Handler**: Catches SIGSEGV and identifies the faulting page address
4. **Page Lookup**: Checks if the faulting page has already been mapped (prevents double-mapping)
5. **Segment Identification**: Finds which segment contains the faulting address
6. **Lazy Page Allocation**: Maps ONLY the specific 4KB page that faulted using mmap with MAP_FIXED
7. **Data Loading**: Copies the appropriate portion of the segment from the ELF file into the mapped page
8. **Resume Execution**: Signal handler returns and program continues from the faulting instruction
9. **Repeat**: Process continues on-demand for each new page access until program completes

**Key Feature**: Pages are allocated in any order based on actual access patterns, not sequentially. This is true lazy loading - if code jumps to page 5 before page 1, only page 5 is loaded.

## Files

- `loader.c` - Main loader implementation
- `Makefile` - Build configuration
- `fib_40.c` - Fibonacci test case (recursive)
- `fib_35.c` - Fibonacci test case (recursive)
- `fib_30.c` - Fibonacci test case (recursive)
- `fib_25.c` - Fibonacci test case (recursive)
- `fib_20.c` - Fibonacci test case (recursive)
- `fib_15.c` - Fibonacci test case (recursive)
- `fib_10.c` - Fibonacci test case (recursive)
- `sum.c` - Array summation test case
- `README.md` - This file
- `DESIGN.md` - Design document with implementation details

## Technical Details

- **Page Size**: 4096 bytes (4KB)
- **Supported**: ELF 32-bit executables only
- **Target**: Programs without glibc dependencies (nostdlib)
- **Compilation Flags**: `-m32 -no-pie -nostdlib` for test cases

## Fragmentation Calculation

Internal fragmentation is calculated only for the last page of each segment:
```
fragmentation = (PAGE_SIZE - (segment_size % PAGE_SIZE))
```
Only counted when all pages for a segment have been allocated.

## Cleaning Up

```bash
make clean
```

This removes all compiled executables (loader, fib_40, fib_30, fib_20, fib_10, fib_35, fib_25, fib_15, sum).
