all: smallsh.c
	rm -f smallsh
	gcc -std=c11 -Wall -Werror -g3 -O0 -o smallsh smallsh.c

clean:
	rm -f smallsh
