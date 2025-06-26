#ifndef __ARRAY_H__
#define __ARRAY_H__

typedef struct {

	void ** data;
	int length;
} t_array;

t_array * arrayNew(int length);
void * arrayGet(t_array * array, int index);
void arraySet(t_array * array, int index, void * value);
void arrayInc(t_array * array, int index);
void arrayDec(t_array * array, int index);
void arrayIncBy(t_array * array, int index, unsigned int amount);
void arrayDecBy(t_array * array, int index, unsigned int amount);
void arrayClear(t_array * array);
void arrayFree(t_array * array);
int arrayLength(t_array * array);
void arrayCopy(t_array * dst, t_array* src);

#endif

