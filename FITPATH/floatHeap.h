#ifndef __FLOATHEAP_H__
#define __FLOATHEAP_H__

#include "graph.h"
#include "list.h"

typedef struct {

	int bufferSize;
	int size;
	void ** elements;
	double * keys;
} t_floatHeap;

t_floatHeap * floatHeapNew();
void floatHeapAdd(t_floatHeap * floatHeap, void * element, double key);
void * floatHeapExtractMinimum(t_floatHeap * floatHeap, double * key);
void * floatHeapTop(t_floatHeap * floatHeap);
double floatHeapTopKey(t_floatHeap * floatHeap);
int floatHeapSize(t_floatHeap * floatHeap);
int floatHeapIsEmpty(t_floatHeap * floatHeap);
void floatHeapFree(t_floatHeap * floatHeap);
void floatHeapFreeWithData(t_floatHeap * floatHeap);
void floatHeapPrint(t_floatHeap * floatHeap);
t_list * floatHeapListify(t_floatHeap * floatHeap);

#endif

