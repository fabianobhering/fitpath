#ifndef __STATEH_H__
#define __STATEH_H__

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
	t_weight * waitingSince;
	unsigned long * backoffBitmap;
	unsigned long * retries;
	t_weight currentTime;
	double deliveredPackets;
	t_array * deliveredPacketsFlows;
	t_array * sentPacketsFlows;
	t_array * delayFlows;
} t_state;

typedef struct {

	t_array * hashTable;
	t_set * usedSlots;
	int hashSize;
} t_stateStorage;

t_state * stateNew(unsigned int slots, int numberOfFlows);
void stateAddTransmission(t_state * state, unsigned long index, t_weight time, unsigned char backoff, unsigned char retries, t_weight waitingSince);
void stateAddBuffer(t_state * state, unsigned long index);
void stateSetCurrentTime(t_state * state, t_weight currentTime);
void stateSetDeliveredPackets(t_state * state, double deliveredPackets);
double stateGetDeliveredPackets(t_state * state);
t_weight stateGetCurrentTime(t_state * state);
t_state * stateLookupAndStore(t_state * state, t_stateStorage * stateStorage);
t_stateStorage * stateStorageNew(int hashSize);
void statePrint(t_state * state);
void stateFree(t_state * state);
void stateStorageFreeWithData(t_stateStorage * stateStorage);
void stateSetDeliveredPacketsFlows(t_state * state, t_array * deliveredPacketsFlows);
t_array * stateGetDeliveredPacketsFlows(t_state * state);
void stateSetSentPacketsFlows(t_state * state, t_array * sentPacketsFlows);
t_array * stateGetSentPacketsFlows(t_state * state);
void stateSetDelayFlows(t_state * state, t_array * delayFlows);
t_array * stateGetDelayFlows(t_state * state);

#endif

