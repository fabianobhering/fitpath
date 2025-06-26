#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "floatHeap.h"
#include "memory.h"
#include "array.h"

#define MIN_ELEMENT_BUFFER		10
#define ELEMENT_BUFFER_INCREASE 10

t_floatHeap * floatHeapNew() {

	t_floatHeap * floatHeap;

	MALLOC(floatHeap, sizeof(t_floatHeap));

	floatHeap->bufferSize = MIN_ELEMENT_BUFFER;
	MALLOC(floatHeap->elements, sizeof(void *) * floatHeap->bufferSize);
	MALLOC(floatHeap->keys, sizeof(double) * floatHeap->bufferSize);
	floatHeap->size = 0;

	return(floatHeap);
}

void floatHeapAdd(t_floatHeap * floatHeap, void * element, double key) {
	
	int current, father;

	if (floatHeap->size == floatHeap->bufferSize) {

		floatHeap->bufferSize += ELEMENT_BUFFER_INCREASE;
		REALLOC(floatHeap->elements, sizeof(void *) * floatHeap->bufferSize);
		REALLOC(floatHeap->keys, sizeof(double) * floatHeap->bufferSize);
	}

	current = floatHeap->size;
	floatHeap->size++;

	while(current) {

		father = (current - 1) / 2;

		if (key >= floatHeap->keys[father]) break;

		floatHeap->elements[current] = floatHeap->elements[father];
		floatHeap->keys[current] = floatHeap->keys[father];

		current = father;
	}

	floatHeap->elements[current] = element;
	floatHeap->keys[current] = key;
}

void * floatHeapExtractMinimum(t_floatHeap * floatHeap, double * key) {
	
	void * element, * tmp;
	int current, left, right;
	double minimunKey;

	if (floatHeap->size == 0) return(NULL);

	element = floatHeap->elements[0];
	if (key != NULL) * key = floatHeap->keys[0];

	floatHeap->elements[0] = floatHeap->elements[floatHeap->size - 1];
	floatHeap->keys[0] = floatHeap->keys[floatHeap->size - 1];
	floatHeap->size--;

	current = 0;
	while(current < floatHeap->size) {

		left = current * 2 + 1;
		right = current * 2 + 2;
		minimunKey = floatHeap->keys[current];

		if (left < floatHeap->size) {

			if (floatHeap->keys[left] < minimunKey) 
				minimunKey = floatHeap->keys[left];
		}

		if (right < floatHeap->size) {

			if (floatHeap->keys[right] < minimunKey)
				minimunKey = floatHeap->keys[right];
		}

		if (minimunKey == floatHeap->keys[current]) break ;

		if (minimunKey == floatHeap->keys[left]) {

			tmp = floatHeap->elements[left];
			floatHeap->elements[left] = floatHeap->elements[current];
			floatHeap->elements[current] = tmp;

			floatHeap->keys[left] = floatHeap->keys[current];
			floatHeap->keys[current] = minimunKey;

			current = left;
		}
		else {

			tmp = floatHeap->elements[right];
			floatHeap->elements[right] = floatHeap->elements[current];
			floatHeap->elements[current] = tmp;

			floatHeap->keys[right] = floatHeap->keys[current];
			floatHeap->keys[current] = minimunKey;

			current = right;
		}
	}

	return(element);
}

void * floatHeapTop(t_floatHeap * floatHeap) {

	if (floatHeap->size == 0) return(NULL);

	return(floatHeap->elements[floatHeap->size - 1]);
}

double floatHeapTopKey(t_floatHeap * floatHeap) {

	if (floatHeap->size == 0) return(0);

	return(floatHeap->keys[floatHeap->size - 1]);
}

int floatHeapSize(t_floatHeap * floatHeap) {

	return(floatHeap->size);
}

int floatHeapIsEmpty(t_floatHeap * floatHeap) {

	return((floatHeap->size == 0));
}

void floatHeapFree(t_floatHeap * floatHeap) {

	if (floatHeap->bufferSize) {

		free(floatHeap->elements);
		free(floatHeap->keys);
		floatHeap->size = 0;
		floatHeap->bufferSize = 0;
		floatHeap->elements = NULL;
		floatHeap->keys = NULL;
	}
}

void floatHeapFreeWithData(t_floatHeap * floatHeap) {

	int i;

	for (i = 0; i < floatHeap->size; i++) free(floatHeap->elements[i]);
	floatHeapFree(floatHeap);
}

void floatHeapPrint(t_floatHeap * floatHeap) {

	int i;

	printf("Heap has %d elements.\nBuffer size is %d.\n", floatHeapSize(floatHeap), floatHeap->bufferSize);

	for (i = 0; i < floatHeap->size; i++) 
		printf("Index = %d\nData = \"%s\"\nKey = %f\n\n", i, (char *) floatHeap->elements[i], floatHeap->keys[i]);
}

t_list * floatHeapListify(t_floatHeap * floatHeap) {

	t_list * newList;
	void * p;

	newList = listNew();
	while((p = floatHeapExtractMinimum(floatHeap, NULL))) {

		listAdd(newList, p);
	}

	return(newList);
}

t_array * floatHeapArrayize(t_floatHeap * floatHeap) {

	t_array * newArray;
	void * p;
	int i;

	newArray = arrayNew(floatHeapSize(floatHeap));
	i = 0;
	while((p = floatHeapExtractMinimum(floatHeap, NULL))) {

		arraySet(newArray, i++, p);
	}

	return(newArray);
}

