#include "array.h"
#include "memory.h"

#include <string.h>

t_array * arrayNew(int length) {

	t_array * array;

	MALLOC(array, sizeof(t_array));
	MALLOC(array->data, length * sizeof(void *));

	array->length = length;

	return(array);
}

inline void * arrayGet(t_array * array, int index) {

	return(array->data[index]);
}


void arraySet(t_array * array, int index, void * value) {

	array->data[index] = value;
}

void arrayInc(t_array * array, int index) {

	unsigned long x;

	x = (unsigned long) array->data[index];
	x++;
	array->data[index] = (void *) x;
}

void arrayDec(t_array * array, int index) {

	unsigned long x;

	x = (unsigned long) array->data[index];
	x--;
	array->data[index] = (void *) x;
}

void arrayIncBy(t_array * array, int index, unsigned int amount) {

	unsigned long x;

	x = (unsigned long) array->data[index];
	x += amount;
	array->data[index] = (void *) x;
}

void arrayDecBy(t_array * array, int index, unsigned int amount) {

	unsigned long x;

	x = (unsigned long) array->data[index];
	x -= amount;
	array->data[index] = (void *) x;
}

void arrayClear(t_array * array) {

	memset(array->data, 0, array->length * sizeof(void *));
}

int arrayLength(t_array * array) {

	return(array->length);
}

void arrayFree(t_array * array) {

	free(array->data);
}

void arrayFreeWithData(t_array * array) {

	int i;

	for (i = 0; i < array->length; i++) free(array->data[i]);
	arrayFree(array);
}

void arrayCopy(t_array * dst, t_array* src) {

	int min;

	if (dst->length < src->length) min = dst->length;
	else min = src->length;

	memcpy(dst->data, src->data, min * sizeof(void *));
}

