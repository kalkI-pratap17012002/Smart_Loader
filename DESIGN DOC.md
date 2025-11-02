# SimpleSmartLoader Design Document


## Assignment Submission Information

**Course**: CSE 231 - Operating Systems 
**Assignment**: Assignment 4 - SimpleSmartLoader (2025) 
**Section**: A 
**Submission Date**: November 2, 2025

---

## Group Information

**Group ID: 13**

### Team Members

**Member:**
- **Name**: Ayush Kumar
- **Roll Number**: 2020290
- **Email**: ayush20290@iiitd.ac.in

---

## Contribution Breakdown

### [Ayush]
- Implemented the signal handler (`handle_segmentation_fault()`)
- Developed page-by-page allocation logic
- Created page tracking linked list (`allocated_page` structure)
- Implemented fragmentation calculation
- Tested with all fib and sum test cases
- Created documentation and comments
- Implemented ELF loading (`load_and_execute_elf()`)
- Developed ELF header validation logic
- Created cleanup function (`cleanup_loader()`)
- Implemented helper function (`is_page_mapped()`)
- Wrote Makefile and build system
- Created README.md and testing documentation


---

## GitHub Repository

**Repository URL**: [https://github.com/kalkI-pratap17012002/Assignment_4-Smart_Loader.git]

**Repository Status**: PRIVATE



## Development Environment

**Primary Development**:
- OS: [e.g., Ubuntu 22.04 LTS / WSL2 Ubuntu]
- Compiler: GCC version [output of: gcc --version]
- Architecture: x86_64 (compiling for 32-bit with -m32)

**Testing Environment**:
- Same as development
- Page size: 4096 bytes


## Implementation Highlights

### Key Features Implemented:
1. ✅ True lazy loading - no upfront segment allocation
2. ✅ Page-by-page allocation (4KB per fault)
3. ✅ Signal-based page fault handling (SIGSEGV)
4. ✅ Double-mapping prevention
5. ✅ Accurate fragmentation calculation
6. ✅ Complete memory cleanup
7. ✅ Comprehensive error handling

### Critical Decisions:
- Used linked list for page tracking (enables O(n) cleanup)
- Deferred program header reading until first fault
- Calculated fragmentation only on last page of each segment
- Implemented MAP_FIXED for precise page placement

---

## Known Limitations

1. **32-bit Only**: Does not support 64-bit ELF files (as per assignment)
2. **No glibc**: Test programs must use -nostdlib (as per assignment)
3. **Linux Only**: Requires Linux system calls (mmap, sigaction)


---

## References

- Linux man pages: mmap(2), munmap(2), sigaction(2), signal(7)
- ELF Specification: Tool Interface Standard (TIS) Executable and Linking Format (ELF) Specification Version 1.2
- Course Lecture Slides: Virtual Memory and Page Fault Handling
- Operating Systems: Three Easy Pieces - Chapter on Memory Virtualization
- Assignment 4 Specification (2025)


---


## Implementation Overview

We have implemented a static SimpleSmartLoader that performs lazy, page-by-page loading of 32-bit ELF executables...


