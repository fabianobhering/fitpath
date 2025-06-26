#ifndef __SET_H__
#define __SET_H__

#include "list.h"

typedef struct {

	int entries;
	int bitsPerEntry;
	int maxElement;
	int numElements;
	unsigned int * elements;
} t_set;

t_set * setNew();
void setAdd(t_set * set, int element);
void setDel(t_set * set, unsigned int element);
void setClear(t_set * set);
int setSize(t_set * set);
int setIsEmpty(t_set * set);
int setIsElementOf(t_set * set, unsigned int element);
void setFree(t_set * set);
void setPrint(t_set * set);
t_list * setGetElements(t_set * set);

#endif

