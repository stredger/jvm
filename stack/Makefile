CC=gcc
CFLAGS=-Wall -c
SRC=stack.c
OBJ=$(SRC:.c=.o) 

all: stack

stack: stack.o
	$(CC) stack.o -o stack

#.c.o:
#	$(CC) $(CFLAGS) $< -o $@

stack.o: stack.c stack.h
	$(CC) $(CFLAGS) stack.c -o stack.o

clean:
	rm -f $(OBJ) stack

# stuff for flymake
.PHONY: check-syntax

check-syntax:
	$(CC) -Wall -0pedantic -fsyntax-only stack.c
