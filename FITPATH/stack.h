#ifndef __STACK_H__
#define __STACK_H__

typedef struct {

	int bufferSize;
	int top;
	void ** elements;
} t_stack;

t_stack * stackNew();
void stackPush(t_stack * stack, void * element);
void * stackPop(t_stack * stack);
void * stackTop(t_stack * stack);
int stackSize(t_stack * stack);
int stackIsEmpty(t_stack * stack);
void stackClear(t_stack * stack);
void stackFree(t_stack * stack);
void stackFreeWithData(t_stack * stack);
void stackPrint(t_stack * stack);

#endif

