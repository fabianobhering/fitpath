#define _GNU_SOURCE
#include "stateh2.h"
#include "memory.h"
#include "array.h"
#include "list.h"

#include <string.h>
#include <strings.h>

t_state * stateNew(unsigned int slots, int numberOfFlows) {

	t_state * state;
	unsigned long entriesNeeded, bytesNeeded;
	
	entriesNeeded = slots / (8 * sizeof(unsigned long));
	if (slots % (8 * sizeof(unsigned long))) entriesNeeded++;
	bytesNeeded = entriesNeeded * sizeof(unsigned long);

	MALLOC(state, sizeof(t_state));
	MALLOC(state->transmissionBitmap, bytesNeeded);
	MALLOC(state->backoffBitmap, bytesNeeded);
	MALLOC(state->retries, 2 * bytesNeeded);
	MALLOC(state->times, sizeof(t_weight) * slots);
	MALLOC(state->waitingSince, sizeof(t_weight) * slots);
	state->bufferList = listNew();

	memset(state->transmissionBitmap, 0, bytesNeeded);
	memset(state->backoffBitmap, 0, bytesNeeded);
	memset(state->retries, 0, 2 * bytesNeeded);
	memset(state->times, 0, sizeof(t_weight) * slots);
	memset(state->waitingSince, 0, sizeof(t_weight) * slots);

	state->deliveredPackets = 0;
	state->currentTime = 0.0;
	state->entries = entriesNeeded;
	state->slots = slots;

	state->deliveredPacketsFlows = arrayNew(numberOfFlows);
	for (int i = 0; i < numberOfFlows; i++) arraySet(state->deliveredPacketsFlows, i, 0);
	
	state->sentPacketsFlows = arrayNew(numberOfFlows);
	for (int i = 0; i < numberOfFlows; i++) arraySet(state->sentPacketsFlows, i, 0);
	
	state->delayFlows = arrayNew(numberOfFlows);
	for (int i = 0; i < numberOfFlows; i++) arraySet(state->delayFlows, i, 0);

	return(state);
}

void stateAddTransmission(t_state * state, unsigned long index, t_weight time, unsigned char backoff, unsigned char retries, t_weight waitingSince) {

	int entryIndex, bitIndex;

	entryIndex = index / (8 * sizeof(unsigned long));
	bitIndex = index % (8 * sizeof(unsigned long));

	state->transmissionBitmap[entryIndex] |= (1 << bitIndex);

	state->times[index] = time;
	state->waitingSince[index] = waitingSince;

	state->backoffBitmap[entryIndex] |= (((unsigned long) backoff) << bitIndex);

	entryIndex = (2 * index) / (8 * sizeof(unsigned long));
	bitIndex = (2 * index) % (8 * sizeof(unsigned long));

	state->retries[entryIndex] &= ~(3ul << bitIndex);
	state->retries[entryIndex] |= (retries << bitIndex);
}

void stateAddBuffer(t_state * state, unsigned long index) {

	listAdd(state->bufferList, (void *) index + 1);
}

void stateSetCurrentTime(t_state * state, t_weight currentTime) {

	state->currentTime = currentTime;
}

void stateSetDeliveredPackets(t_state * state, double deliveredPackets) {

	state->deliveredPackets = deliveredPackets;
}

double stateGetDeliveredPackets(t_state * state) {

	return(state->deliveredPackets);
}

void stateSetDeliveredPacketsFlows(t_state * state, t_array * deliveredPacketsFlows) {

	for (int i = 0; i < arrayLength(deliveredPacketsFlows); i++)
		arraySet(state->deliveredPacketsFlows, i, arrayGet(deliveredPacketsFlows,i));
}

t_array * stateGetDeliveredPacketsFlows(t_state * state) {

	return(state->deliveredPacketsFlows);
}

void stateSetSentPacketsFlows(t_state * state, t_array * sentPacketsFlows) {

	for (int i = 0; i < arrayLength(sentPacketsFlows); i++) 
		arraySet(state->sentPacketsFlows, i, arrayGet(sentPacketsFlows,i));
}

t_array * stateGetSentPacketsFlows(t_state * state) {

	return(state->sentPacketsFlows);
}

void stateSetDelayFlows(t_state * state, t_array * delayFlows) {

	for (int i = 0; i < arrayLength(delayFlows); i++) 
		arraySet(state->delayFlows, i, arrayGet(delayFlows,i));
}

t_array * stateGetDelayFlows(t_state * state) {

	return(state->delayFlows);
}

t_weight stateGetCurrentTime(t_state * state) {

	return(state->currentTime);
}

unsigned long stateComputeHash(t_state * state) {

	static const unsigned int table[256] = {
	0x00000000, 0x015D6DCB, 0x02BADB96, 0x03E7B65D, 
	0x0528DAE7, 0x0475B72C, 0x07920171, 0x06CF6CBA, 
	0x0A51B5CE, 0x0B0CD805, 0x08EB6E58, 0x09B60393, 
	0x0F796F29, 0x0E2402E2, 0x0DC3B4BF, 0x0C9ED974, 
	0x14A36B9C, 0x15FE0657, 0x1619B00A, 0x1744DDC1, 
	0x118BB17B, 0x10D6DCB0, 0x13316AED, 0x126C0726, 
	0x1EF2DE52, 0x1FAFB399, 0x1C4805C4, 0x1D15680F, 
	0x1BDA04B5, 0x1A87697E, 0x1960DF23, 0x183DB2E8, 
	0x291BBAF3, 0x2846D738, 0x2BA16165, 0x2AFC0CAE, 
	0x2C336014, 0x2D6E0DDF, 0x2E89BB82, 0x2FD4D649, 
	0x234A0F3D, 0x221762F6, 0x21F0D4AB, 0x20ADB960, 
	0x2662D5DA, 0x273FB811, 0x24D80E4C, 0x25856387, 
	0x3DB8D16F, 0x3CE5BCA4, 0x3F020AF9, 0x3E5F6732, 
	0x38900B88, 0x39CD6643, 0x3A2AD01E, 0x3B77BDD5, 
	0x37E964A1, 0x36B4096A, 0x3553BF37, 0x340ED2FC, 
	0x32C1BE46, 0x339CD38D, 0x307B65D0, 0x3126081B, 
	0x523775E6, 0x536A182D, 0x508DAE70, 0x51D0C3BB, 
	0x571FAF01, 0x5642C2CA, 0x55A57497, 0x54F8195C, 
	0x5866C028, 0x593BADE3, 0x5ADC1BBE, 0x5B817675, 
	0x5D4E1ACF, 0x5C137704, 0x5FF4C159, 0x5EA9AC92, 
	0x46941E7A, 0x47C973B1, 0x442EC5EC, 0x4573A827, 
	0x43BCC49D, 0x42E1A956, 0x41061F0B, 0x405B72C0, 
	0x4CC5ABB4, 0x4D98C67F, 0x4E7F7022, 0x4F221DE9, 
	0x49ED7153, 0x48B01C98, 0x4B57AAC5, 0x4A0AC70E, 
	0x7B2CCF15, 0x7A71A2DE, 0x79961483, 0x78CB7948, 
	0x7E0415F2, 0x7F597839, 0x7CBECE64, 0x7DE3A3AF, 
	0x717D7ADB, 0x70201710, 0x73C7A14D, 0x729ACC86, 
	0x7455A03C, 0x7508CDF7, 0x76EF7BAA, 0x77B21661, 
	0x6F8FA489, 0x6ED2C942, 0x6D357F1F, 0x6C6812D4, 
	0x6AA77E6E, 0x6BFA13A5, 0x681DA5F8, 0x6940C833, 
	0x65DE1147, 0x64837C8C, 0x6764CAD1, 0x6639A71A, 
	0x60F6CBA0, 0x61ABA66B, 0x624C1036, 0x63117DFD, 
	0xA46EEBCC, 0xA5338607, 0xA6D4305A, 0xA7895D91, 
	0xA146312B, 0xA01B5CE0, 0xA3FCEABD, 0xA2A18776, 
	0xAE3F5E02, 0xAF6233C9, 0xAC858594, 0xADD8E85F, 
	0xAB1784E5, 0xAA4AE92E, 0xA9AD5F73, 0xA8F032B8, 
	0xB0CD8050, 0xB190ED9B, 0xB2775BC6, 0xB32A360D, 
	0xB5E55AB7, 0xB4B8377C, 0xB75F8121, 0xB602ECEA, 
	0xBA9C359E, 0xBBC15855, 0xB826EE08, 0xB97B83C3, 
	0xBFB4EF79, 0xBEE982B2, 0xBD0E34EF, 0xBC535924, 
	0x8D75513F, 0x8C283CF4, 0x8FCF8AA9, 0x8E92E762, 
	0x885D8BD8, 0x8900E613, 0x8AE7504E, 0x8BBA3D85, 
	0x8724E4F1, 0x8679893A, 0x859E3F67, 0x84C352AC, 
	0x820C3E16, 0x835153DD, 0x80B6E580, 0x81EB884B, 
	0x99D63AA3, 0x988B5768, 0x9B6CE135, 0x9A318CFE, 
	0x9CFEE044, 0x9DA38D8F, 0x9E443BD2, 0x9F195619, 
	0x93878F6D, 0x92DAE2A6, 0x913D54FB, 0x90603930, 
	0x96AF558A, 0x97F23841, 0x94158E1C, 0x9548E3D7, 
	0xF6599E2A, 0xF704F3E1, 0xF4E345BC, 0xF5BE2877, 
	0xF37144CD, 0xF22C2906, 0xF1CB9F5B, 0xF096F290, 
	0xFC082BE4, 0xFD55462F, 0xFEB2F072, 0xFFEF9DB9, 
	0xF920F103, 0xF87D9CC8, 0xFB9A2A95, 0xFAC7475E, 
	0xE2FAF5B6, 0xE3A7987D, 0xE0402E20, 0xE11D43EB, 
	0xE7D22F51, 0xE68F429A, 0xE568F4C7, 0xE435990C, 
	0xE8AB4078, 0xE9F62DB3, 0xEA119BEE, 0xEB4CF625, 
	0xED839A9F, 0xECDEF754, 0xEF394109, 0xEE642CC2, 
	0xDF4224D9, 0xDE1F4912, 0xDDF8FF4F, 0xDCA59284, 
	0xDA6AFE3E, 0xDB3793F5, 0xD8D025A8, 0xD98D4863, 
	0xD5139117, 0xD44EFCDC, 0xD7A94A81, 0xD6F4274A, 
	0xD03B4BF0, 0xD166263B, 0xD2819066, 0xD3DCFDAD, 
	0xCBE14F45, 0xCABC228E, 0xC95B94D3, 0xC806F918, 
	0xCEC995A2, 0xCF94F869, 0xCC734E34, 0xCD2E23FF, 
	0xC1B0FA8B, 0xC0ED9740, 0xC30A211D, 0xC2574CD6, 
	0xC498206C, 0xC5C54DA7, 0xC622FBFA, 0xC77F9631, 
	};
	unsigned long crc = 0;
	unsigned long element;
	int i;
	int j;
	unsigned char * data;

	for (i = 0; i < state->entries; i++) {

		data = & (state->transmissionBitmap[i]);
		for (j = 0; j < sizeof(unsigned long); j++)
			crc = table[data[j] ^ ((crc >> 16) & 0xff)] ^ (crc << 8);
	}

	for (i = 0; i < state->entries; i++) {

		data = & (state->backoffBitmap[i]);
		for (j = 0; j < sizeof(unsigned long); j++)
			crc = table[data[j] ^ ((crc >> 16) & 0xff)] ^ (crc << 8);
	}

	for (i = 0; i < 2 * state->entries; i++) {

		data = & (state->retries[i]);
		for (j = 0; j < sizeof(unsigned long); j++)
			crc = table[data[j] ^ ((crc >> 16) & 0xff)] ^ (crc << 8);
	}

	for (i = 0; i < state->slots; i++) {

		data = & (state->times[i]);
		for (j = 0; j < sizeof(t_weight); j++)
			crc = table[data[j] ^ ((crc >> 16) & 0xff)] ^ (crc << 8);
	}

	for (i = 0; i < state->slots; i++) {

		data = & (state->waitingSince[i]);
		for (j = 0; j < sizeof(t_weight); j++)
			crc = table[data[j] ^ ((crc >> 16) & 0xff)] ^ (crc << 8);
	}

	for (element = (unsigned long) listBegin(state->bufferList); 
		element; 
		element = (unsigned long) listNext(state->bufferList)) {

		data = & (element);
		for (j = 0; j < sizeof(unsigned long); j++)
			crc = table[data[j] ^ ((crc >> 16) & 0xff)] ^ (crc << 8);
		
	}

	return(crc & 0xFFFFFF);
}

t_state * stateLookupAndStore(t_state * state, t_stateStorage * stateStorage) {

	unsigned long hash, element1, element2;
	t_list * hashEntry;
	t_state * pState;

//statePrint(state);
	hash = stateComputeHash(state);
	hashEntry = arrayGet(stateStorage->hashTable, hash);
	if (hashEntry) {

		for (pState = listBegin(hashEntry); pState; pState = listNext(hashEntry)) {

			if (memcmp(pState->transmissionBitmap, state->transmissionBitmap, sizeof(unsigned long) * state->entries))
				continue ;
			if (memcmp(pState->backoffBitmap, state->backoffBitmap, sizeof(unsigned long) * state->entries))
				continue ;
			if (memcmp(pState->retries, state->retries, 2 * sizeof(unsigned long) * state->entries))
				continue ;
			if (memcmp(pState->times, state->times, sizeof(t_weight) * state->slots))
				continue ;
			if (memcmp(pState->waitingSince, state->waitingSince, sizeof(t_weight) * state->slots))
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

	int i, bitIndex, transmissionIndex, retryIndex, retryBit;
	unsigned long x;
	unsigned long backoff;
	unsigned char retry;

	for (i = 0; i < state->entries; i++) {

		x = state->transmissionBitmap[i];
		while(x) {

			bitIndex = ffsl(x) - 1;
			x = x & (x - 1);
			transmissionIndex = bitIndex + i * 8 * sizeof(unsigned long);
			backoff = (state->backoffBitmap[i] & (1ul << bitIndex)) >> bitIndex;
			retryIndex = (2 * transmissionIndex) / (8 * sizeof(unsigned long));
			retryBit = (2 * transmissionIndex) % (8 * sizeof(unsigned long));
			retry = (state->retries[retryIndex] & (3 << retryBit)) >> retryBit;
			printf("%d|" WEIGHT_FORMAT "*" WEIGHT_FORMAT "*%lu*%hhu;", transmissionIndex, state->times[transmissionIndex], state->waitingSince[transmissionIndex], backoff, retry);
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
	free(state->backoffBitmap);
	free(state->retries);
	listFree(state->bufferList);
	free(state->bufferList);
	free(state->times);
	free(state->waitingSince);
	arrayFree(state->deliveredPacketsFlows);
	free(state->deliveredPacketsFlows);
	arrayFree(state->sentPacketsFlows);
	free(state->sentPacketsFlows);
	arrayFree(state->delayFlows);
	free(state->delayFlows);
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


