
#include "stack.h"
#include <stdio.h>

int push(void **ele, stack *s) {

  if (s->head == &s->data[STACK_SIZE])
    return -1;
  *s->head++ = *ele;
  //printf("%d\n", *ele);
  //printf("%d\n", *(s->head-1));
  //printf("%d", (int) s->head);
  return 0;
}


void *peek(stack *s) {
  if (s->head == s->data)
    return 0;
  //printf("%d", (int) s->head);
  return *(s->head-1);
}


void *pop(stack *s) {
  if (s->head == s->data)
    return 0;
  return *(--s->head);
}


stack *create_stack() {
  stack *s = (stack*) malloc(sizeof(stack));
  s->head = s->data;
  return s;
}


int main(int argc, char *argv[]) {

  stack *s = create_stack();
  int i = 1;
  int *j = &i;

  push((void*)&j,s);
  printf("%d", *( (int*)peek(s)));

  return 0;
}
