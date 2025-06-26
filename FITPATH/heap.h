#ifndef __HEAP_H__
#define __HEAP_H__

#include "graph.h"
#include "list.h"

typedef struct {

	int bufferSize;
	int size;
	void ** elements;
	t_weight * keys;
} t_heap;

t_heap * heapNew();
void heapAdd(t_heap * heap, void * element, t_weight key);
void * heapExtractMinimum(t_heap * heap, t_weight * key);
void * heapTop(t_heap * heap);
t_weight heapTopKey(t_heap * heap);
int heapSize(t_heap * heap);
int heapIsEmpty(t_heap * heap);
void heapFree(t_heap * heap);
void heapFreeWithData(t_heap * heap);
void heapPrint(t_heap * heap);
t_list * heapListify(t_heap * heap);

#endif

