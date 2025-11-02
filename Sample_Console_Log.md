dell@dell-Inspiron-3537:~/Desktop/CLAUDE/CLAUDE$ make clean
dell@dell-Inspiron-3537:~/Desktop/CLAUDE/CLAUDE$ make
gcc -m32 -o loader loader.c
gcc -m32 -no-pie -nostdlib -o fib_40 fib_40.c
gcc -m32 -no-pie -nostdlib -o fib_20 fib_20.c
gcc -m32 -no-pie -nostdlib -o fib_25 fib_25.c
gcc -m32 -no-pie -nostdlib -o fib_10 fib_10.c
gcc -m32 -no-pie -nostdlib -o fib_15 fib_15.c
gcc -m32 -no-pie -nostdlib -o fib_35 fib_35.c
gcc -m32 -no-pie -nostdlib -o sum sum.c
dell@dell-Inspiron-3537:~/Desktop/CLAUDE/CLAUDE$ ./loader ./sum
Starting ELF execution...
Program returned: 2048

Execution Statistics:
Total Page Faults: 3
Total Page Allocations: 3
Internal Fragmentation: 8066 Bytes (7.877 KB)
dell@dell-Inspiron-3537:~/Desktop/CLAUDE/CLAUDE$ ./loader ./fib_10
Starting ELF execution...
Program returned: 55

Execution Statistics:
Total Page Faults: 1
Total Page Allocations: 1
Internal Fragmentation: 3982 Bytes (3.889 KB)
dell@dell-Inspiron-3537:~/Desktop/CLAUDE/CLAUDE$ ./loader ./fib_15
Starting ELF execution...
Program returned: 610

Execution Statistics:
Total Page Faults: 1
Total Page Allocations: 1
Internal Fragmentation: 3982 Bytes (3.889 KB)
dell@dell-Inspiron-3537:~/Desktop/CLAUDE/CLAUDE$ ./loader ./fib_35
Starting ELF execution...
Program returned: 9227465

Execution Statistics:
Total Page Faults: 1
Total Page Allocations: 1
Internal Fragmentation: 3982 Bytes (3.889 KB)
dell@dell-Inspiron-3537:~/Desktop/CLAUDE/CLAUDE$ ./loader ./fib_40
Starting ELF execution...
Program returned: 102334155

Execution Statistics:
Total Page Faults: 1
Total Page Allocations: 1
Internal Fragmentation: 3982 Bytes (3.889 KB)
dell@dell-Inspiron-3537:~/Desktop/CLAUDE/CLAUDE$ 
