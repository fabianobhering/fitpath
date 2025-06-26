#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "stack.h"
#include "memory.h"

#define MIN_ELEMENT_BUFFER		10
#define ELEMENT_BUFFER_INCREASE 10

t_stack * stackNew() {

	t_stack * stack;

	MALLOC(stack, sizeof(t_stack));

	stack->bufferSize = MIN_ELEMENT_BUFFER;
	MALLOC(stack->elements, sizeof(void *) * stack->bufferSize);
	stack->top = -1;

	return(stack);
}

void stackPush(t_stack * stack, void * element) {

	if (stack->top == stack->bufferSize - 1) {

		stack->bufferSize += ELEMENT_BUFFER_INCREASE;
		REALLOC(stack->elements, sizeof(void *) * stack->bufferSize);
	}

	stack->top++;
	stack->elements[stack->top] = element;
}

void * stackPop(t_stack * stack) {
	
	if (stack->top < 0) return(NULL);

	stack->top--;
	return(stack->elements[stack->top + 1]);
}

void * stackTop(t_stack * stack) {

	if (stack->top < 0) return(NULL);

	return(stack->elements[stack->top]);
}

int stackSize(t_stack * stack) {

	return(stack->top + 1);
}

int stackIsEmpty(t_stack * stack) {

	return((stack->top < 0));
}

void stackClear(t_stack * stack) {

	stack->top = -1;
}

void stackFree(t_stack * stack) {

	if (stack->bufferSize) {

		free(stack->elements);
		stack->top = -1;
		stack->bufferSize = 0;
		stack->elements = NULL;
	}
}

void stackFreeWithData(t_stack * stack) {

	int i;

	for (i = 0; i <= stack->top; i++) free(stack->elements[i]);
	stackFree(stack);
}

void stackPrint(t_stack * stack) {

	int i;

	printf("Stack has %d elements.\nBuffer size is %d.\n", stackSize(stack), stack->bufferSize);

	for (i = stack->top; i >= 0; i--) printf("Data = \"%s\"\n", (char *) stack->elements[i]);
}

