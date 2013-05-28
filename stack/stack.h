
#include <stdlib.h>

#define STACK_SIZE 65536

typedef struct stack_t {
  int *data[STACK_SIZE];
  int **head;
} stack;

stack *create_stack();
int push(int **ele, stack *s);
int *peek(stack *s);
int *pop(stack *s);

