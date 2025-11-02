all:
	gcc -m32 -o loader loader.c
	gcc -m32 -no-pie -nostdlib -o fib_40 fib_40.c
	gcc -m32 -no-pie -nostdlib -o fib_20 fib_20.c
	gcc -m32 -no-pie -nostdlib -o fib_25 fib_25.c
	gcc -m32 -no-pie -nostdlib -o fib_10 fib_10.c
	gcc -m32 -no-pie -nostdlib -o fib_15 fib_15.c
	gcc -m32 -no-pie -nostdlib -o fib_35 fib_35.c
	gcc -m32 -no-pie -nostdlib -o sum sum.c

clean:
	-@rm -f loader fib_10 fib_20 fib_30 fib_40 fib_15 fib_25 fib_35 sum
