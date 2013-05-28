
#include <stdlib.h>

#define STACK_SIZE 65536

typedef struct stack_t {
  void *data[STACK_SIZE];
  void **head;
} stack;

stack *create_stack();
int push(void **ele, stack *s);
void *peek(stack *s);
void *pop(stack *s);

