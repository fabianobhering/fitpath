#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "set.h"
#include "memory.h"
#include "list.h"

#define MIN_ENTRIES			10
#define ENTRIES_INCREMENT 	10

t_set * setNew() {

	t_set * set;

	MALLOC(set, sizeof(t_set));

	set->bitsPerEntry = 8 * sizeof(unsigned int);

	set->entries = MIN_ENTRIES;
	set->maxElement = set->bitsPerEntry * set->entries - 1;
	set->numElements = 0;
	MALLOC(set->elements, sizeof(unsigned int) * set->entries);
	memset(set->elements, 0, set->entries * sizeof(unsigned int));

	return(set);
}

void setAdd(t_set * set, int element) {

	int entryIndex, bitIndex;

	entryIndex = element / set->bitsPerEntry;
	bitIndex = element % set->bitsPerEntry;

	if (element > set->maxElement) {

		REALLOC(set->elements, sizeof(unsigned int) * (entryIndex + 1 + ENTRIES_INCREMENT));
		memset(& (set->elements[set->entries]), 0, sizeof(unsigned int) * (entryIndex + 1 + ENTRIES_INCREMENT - set->entries));
		set->entries = entryIndex + 1 + ENTRIES_INCREMENT;
		set->maxElement = set->bitsPerEntry * set->entries - 1;
	}

	if (set->elements[entryIndex] & (1u << bitIndex)) return;

	set->elements[entryIndex] |= (1u << bitIndex);
	set->numElements++;
}

void setDel(t_set * set, unsigned int element) {
	
	int entryIndex, bitIndex;

	if (element > set->maxElement) return;

	entryIndex = element / set->bitsPerEntry;
	bitIndex = element % set->bitsPerEntry;

	if (set->elements[entryIndex] & (1u << bitIndex)) {
		
		set->elements[entryIndex] &= ~(1u << bitIndex);
		set->numElements--;
	}
}

void setClear(t_set * set) {

	int i;

	for (i = 0; i < set->entries; i++) set->elements[i] = 0;
	set->numElements = 0;
}

int setSize(t_set * set) {

	return(set->numElements);
}

int setIsEmpty(t_set * set) {

	return((set->numElements == 0));
}

int setIsElementOf(t_set * set, unsigned int element) {

	int entryIndex, bitIndex;

	entryIndex = element / set->bitsPerEntry;
	bitIndex = element % set->bitsPerEntry;

	if (element > set->maxElement) return(0);

	if (set->elements[entryIndex] & (1u << bitIndex)) return(1);

	return(0);
}

void setFree(t_set * set) {

	if (set->entries) {

		free(set->elements);
		set->entries = 0;
		set->maxElement = -1;
		set->numElements = 0;
		set->elements = NULL;
	}
}

void setPrint(t_set * set) {

	int i, lsb, base;
	unsigned int x;

	printf("Set has %d elements.\nEntries = %d and maxElement = %d.\n", setSize(set), set->entries, set->maxElement);

	base = -1;
	for (i = 0; i < set->entries; i++) {
	
		x = set->elements[i];
		while(x) {

			lsb = ffsl(x);
			printf("Element %d is present.\n", base + lsb);
			x &= (x - 1);
		}

		base += set->bitsPerEntry;
	}
}

t_list * setGetElements(t_set * set) {

	int i;
	unsigned long lsb, base;
	unsigned int x;
	t_list * list;

	list = listNew();
	base = 0;
	for (i = 0; i < set->entries; i++) {
	
		x = set->elements[i];
		while(x) {

			lsb = ffsl(x);
			x &= (x - 1);
			listAdd(list, (void *) (base + lsb));
		}

		base += set->bitsPerEntry;
	}

	return(list);
}

