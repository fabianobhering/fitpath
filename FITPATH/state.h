#ifndef __STATE_H__
#define __STATE_H__

#include "array.h"
#include "set.h"
#include "graph.h"
#include "list.h"

typedef struct {

	unsigned long * transmissionBitmap;
	t_list * bufferList;
	unsigned long entries;
	unsigned long slots;
	t_weight * times;
	t_weight currentTime;
	unsigned int deliveredPackets;
	unsigned long hash;
} t_state;

typedef struct {

	t_array * hashTable;
	t_set * usedSlots;
	int hashSize;
} t_stateStorage;

t_state * stateNew(unsigned int slots);
void stateAddTransmission(t_state * state, unsigned long index, t_weight time);
void stateAddBuffer(t_state * state, unsigned long index);
void stateSetCurrentTime(t_state * state, t_weight currentTime);
void stateSetDeliveredPackets(t_state * state, unsigned int deliveredPackets);
unsigned int stateGetDeliveredPackets(t_state * state);
t_weight stateGetCurrentTime(t_state * state);
t_state * stateLookupAndStore(t_state * state, t_stateStorage * stateStorage);
t_stateStorage * stateStorageNew(int hashSize);
void statePrint(t_state * state);
void stateFree(t_state * state);
void stateStorageFreeWithData(t_stateStorage * stateStorage);

#endif

