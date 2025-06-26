#define _GNU_SOURCE
#include "state.h"
#include "memory.h"
#include "array.h"
#include "list.h"

#include <string.h>
#include <strings.h>

t_state * stateNew(unsigned int slots) {

	t_state * state;
	unsigned long entriesNeeded, bytesNeeded;
	
	entriesNeeded = slots / (8 * sizeof(unsigned long));
	if (slots % (8 * sizeof(unsigned long))) entriesNeeded++;
	bytesNeeded = entriesNeeded * sizeof(unsigned long);

	MALLOC(state, sizeof(t_state));
	MALLOC(state->transmissionBitmap, bytesNeeded);
	MALLOC(state->times, sizeof(t_weight) * slots);
	state->bufferList = listNew();

	memset(state->transmissionBitmap, 0, bytesNeeded);
	memset(state->times, 0, sizeof(t_weight) * slots);

	state->deliveredPackets = 0;
	state->hash = 0;
	state->currentTime = 0.0;
	state->entries = entriesNeeded;
	state->slots = slots;

	return(state);
}

void stateAddTransmission(t_state * state, unsigned long index, t_weight time) {

	int entryIndex, bitIndex;

	entryIndex = index / (8 * sizeof(unsigned long));
	bitIndex = index % (8 * sizeof(unsigned long));

	state->hash -= state->transmissionBitmap[entryIndex];

	state->transmissionBitmap[entryIndex] |= (1 << bitIndex);
	state->times[index] = time;

	state->hash += state->transmissionBitmap[entryIndex];
	state->hash += (* ((unsigned long *) & time));
}

void stateAddBuffer(t_state * state, unsigned long index) {

	listAdd(state->bufferList, (void *) index + 1);

	state->hash += (index * listLength(state->bufferList));
}

void stateSetCurrentTime(t_state * state, t_weight currentTime) {

	state->currentTime = currentTime;
}

void stateSetDeliveredPackets(t_state * state, unsigned int deliveredPackets) {

	state->deliveredPackets = deliveredPackets;
}

unsigned int stateGetDeliveredPackets(t_state * state) {

	return(state->deliveredPackets);
}

t_weight stateGetCurrentTime(t_state * state) {

	return(state->currentTime);
}

t_state * stateLookupAndStore(t_state * state, t_stateStorage * stateStorage) {

	unsigned long hash, element1, element2;
	t_list * hashEntry;
	t_state * pState;

//statePrint(state);
	hash = state->hash % stateStorage->hashSize;
	hashEntry = arrayGet(stateStorage->hashTable, hash);
	if (hashEntry) {

		for (pState = listBegin(hashEntry); pState; pState = listNext(hashEntry)) {

			if (memcmp(pState->transmissionBitmap, state->transmissionBitmap, sizeof(unsigned long) * state->entries))
				continue ;
			if (memcmp(pState->times, state->times, sizeof(t_weight) * state->slots))
				continue ;

			element1 = (unsigned long) listBegin(pState->bufferList);
			element2 = (unsigned long) listBegin(state->bufferList);
			while(element1 && element2) {

				if (element1 != element2) break ;
				element1 = (unsigned long) listNext(pState->bufferList);
				element2 = (unsigned long) listNext(state->bufferList);
			}
			if (element1 || element2) continue ;

			return(pState);
		}
	}
	else {

		hashEntry = listNew();
		arraySet(stateStorage->hashTable, hash, hashEntry);
		setAdd(stateStorage->usedSlots, hash);
	}

	listAdd(hashEntry, state);
	return(NULL);
}

t_stateStorage * stateStorageNew(int hashSize) {

	t_stateStorage * stateStorage;

	MALLOC(stateStorage, sizeof(t_stateStorage));
	stateStorage->hashTable = arrayNew(hashSize);
	stateStorage->hashSize = hashSize;

	arrayClear(stateStorage->hashTable);

	stateStorage->usedSlots = setNew();

	return(stateStorage);
}

void statePrint(t_state * state) {

	int i, bitIndex, transmissionIndex;
	unsigned long x;

	for (i = 0; i < state->entries; i++) {

		x = state->transmissionBitmap[i];
		while(x) {

			bitIndex = ffsl(x);
			x = x & (x - 1);
			transmissionIndex = bitIndex + i * 8 * sizeof(unsigned long) - 1;
			printf("%d|" WEIGHT_FORMAT ";", transmissionIndex, state->times[transmissionIndex]);
		}
	}
	printf("?");
	for (x = (unsigned long) listBegin(state->bufferList); x; x = (unsigned long) listNext(state->bufferList)) {

		printf("%lu;", x - 1);
	}
	printf("\n");
}

void stateFree(t_state * state) {

	free(state->transmissionBitmap);
	listFree(state->bufferList);
	free(state->bufferList);
	free(state->times);
}

void stateStorageFreeWithData(t_stateStorage * stateStorage) {

	t_list * list;
	t_list * hashEntry;
	t_state * state;
	unsigned long i;

	list = setGetElements(stateStorage->usedSlots);
	for (i = (unsigned long) listBegin(list); i; i = (unsigned long) listNext(list)) {

		i = i - 1;
		hashEntry = arrayGet(stateStorage->hashTable, i);

		for (state = listBegin(hashEntry); state; state = listNext(hashEntry)) {

			stateFree(state);
		}
		listFreeWithData(hashEntry);
		free(hashEntry);
	}
	listFree(list);
	free(list);
	setFree(stateStorage->usedSlots);
	free(stateStorage->usedSlots);
	arrayFree(stateStorage->hashTable);
	free(stateStorage->hashTable);
}

