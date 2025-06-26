#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "heap.h"
#include "memory.h"

#define MIN_ELEMENT_BUFFER		10
#define ELEMENT_BUFFER_INCREASE 10

t_heap * heapNew() {

	t_heap * heap;

	MALLOC(heap, sizeof(t_heap));

	heap->bufferSize = MIN_ELEMENT_BUFFER;
	MALLOC(heap->elements, sizeof(void *) * heap->bufferSize);
	MALLOC(heap->keys, sizeof(t_weight) * heap->bufferSize);
	heap->size = 0;

	return(heap);
}

void heapAdd(t_heap * heap, void * element, t_weight key) {
	
	int current, father;

	if (heap->size == heap->bufferSize) {

		heap->bufferSize += ELEMENT_BUFFER_INCREASE;
		REALLOC(heap->elements, sizeof(void *) * heap->bufferSize);
		REALLOC(heap->keys, sizeof(t_weight) * heap->bufferSize);
	}

	current = heap->size;
	heap->size++;

	while(current) {

		father = (current - 1) / 2;

		if (key >= heap->keys[father]) break;

		heap->elements[current] = heap->elements[father];
		heap->keys[current] = heap->keys[father];

		current = father;
	}

	heap->elements[current] = element;
	heap->keys[current] = key;
}

void * heapExtractMinimum(t_heap * heap, t_weight * key) {
	
	void * element, * tmp;
	int current, left, right;
	t_weight minimunKey;

	if (heap->size == 0) return(NULL);

	element = heap->elements[0];
	if (key != NULL) * key = heap->keys[0];

	heap->elements[0] = heap->elements[heap->size - 1];
	heap->keys[0] = heap->keys[heap->size - 1];
	heap->size--;

	current = 0;
	while(current < heap->size) {

		left = current * 2 + 1;
		right = current * 2 + 2;
		minimunKey = heap->keys[current];

		if (left < heap->size) {

			if (heap->keys[left] < minimunKey) 
				minimunKey = heap->keys[left];
		}

		if (right < heap->size) {

			if (heap->keys[right] < minimunKey)
				minimunKey = heap->keys[right];
		}

		if (minimunKey == heap->keys[current]) break ;

		if (minimunKey == heap->keys[left]) {

			tmp = heap->elements[left];
			heap->elements[left] = heap->elements[current];
			heap->elements[current] = tmp;

			heap->keys[left] = heap->keys[current];
			heap->keys[current] = minimunKey;

			current = left;
		}
		else {

			tmp = heap->elements[right];
			heap->elements[right] = heap->elements[current];
			heap->elements[current] = tmp;

			heap->keys[right] = heap->keys[current];
			heap->keys[current] = minimunKey;

			current = right;
		}
	}

	return(element);
}

void * heapTop(t_heap * heap) {

	if (heap->size == 0) return(NULL);

	return(heap->elements[heap->size - 1]);
}

t_weight heapTopKey(t_heap * heap) {

	if (heap->size == 0) return(0);

	return(heap->keys[heap->size - 1]);
}

int heapSize(t_heap * heap) {

	return(heap->size);
}

int heapIsEmpty(t_heap * heap) {

	return((heap->size == 0));
}

void heapFree(t_heap * heap) {

	if (heap->bufferSize) {

		free(heap->elements);
		free(heap->keys);
		heap->size = 0;
		heap->bufferSize = 0;
		heap->elements = NULL;
		heap->keys = NULL;
	}
}

void heapFreeWithData(t_heap * heap) {

	int i;

	for (i = 0; i < heap->size; i++) free(heap->elements[i]);
	heapFree(heap);
}

void heapPrint(t_heap * heap) {

	int i;

	printf("Heap has %d elements.\nBuffer size is %d.\n", heapSize(heap), heap->bufferSize);

	for (i = 0; i < heap->size; i++) 
		printf("Index = %d\nData = \"%s\"\nKey = " WEIGHT_FORMAT "\n\n", i, (char *) heap->elements[i], heap->keys[i]);
}

t_list * heapListify(t_heap * heap) {

	t_list * newList;
	void * p;

	newList = listNew();
	while((p = heapExtractMinimum(heap, NULL))) {

		listAddAtBegin(newList, p);
	}

	return(newList);
}

