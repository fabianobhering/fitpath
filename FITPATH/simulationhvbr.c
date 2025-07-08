#define _ISOC99_SOURCE
#include <math.h>
#include <string.h>

//#include <sys/times.h>

#include "simulationh2.h"
#include "memory.h"
#include "list.h"
#include "graph.h"
#include "array.h"
#include "stack.h"
#include "stateh2.h"
#include <stdbool.h>

typedef struct {

	t_list * packets;
	t_array * individualFlowCounts;
} t_local_queue;

typedef struct {

	t_local_queue * localQueue;
	t_list * activeNodes;
	int numberOfNodes;
	int queueLimit;
} t_queues;

typedef struct {
	int id;
	int flow;
	t_weight initialTime;
	int currentHop;
	t_weight ETA;
	int lastIteration;
	int retries;
	int maxRetries;
	double deliveryProbability;
	t_weight waitingSince;
} t_packet;

// Descreve um passo do padrão: uma rajada e o tempo de espera seguinte.
typedef struct {
    int num_packets_in_burst; // Quantos pacotes são enviados nesta rajada.
    int inter_burst_interval; // Tempo de espera APÓS o fim desta rajada.
} t_burst_step;

int simulationConflictNodeIndex(int * linkIndexBase, int pathIndex, int linkIndex) {

	return(linkIndexBase[pathIndex] + linkIndex);
}

t_queues * queuesNew(int numberOfNodes, int queueLimit) {

	t_queues * queues;

	MALLOC(queues, sizeof(t_queues));
	MALLOC(queues->localQueue, numberOfNodes * sizeof(t_local_queue));
	memset(queues->localQueue, 0, numberOfNodes * sizeof(t_local_queue));

	queues->activeNodes = listNew();

	queues->numberOfNodes = numberOfNodes;
	queues->queueLimit = queueLimit;

	//printf("queueLimit %d\n",queueLimit);

	return(queues);
}

void queuesAddNode(t_queues * queues, unsigned long node, unsigned long nflows) {

	if (queues->localQueue[node].packets) return;

	queues->localQueue[node].packets = listNew();
	queues->localQueue[node].individualFlowCounts = arrayNew(nflows);
	arrayClear(queues->localQueue[node].individualFlowCounts);

	/*
	 * Avoid 0, as it would be confusing with NULL (end of list).
	 */
	listAdd(queues->activeNodes, (void *) (node + 1));
}

t_list * queuesActiveNodes(t_queues * queues) {

	return(queues->activeNodes);
}

void queuesAddPaths(t_queues * queues, t_array * paths) {

	int numberOfPaths, numberOfNodes;
	int i, j;
	t_array * path;

	numberOfPaths = arrayLength(paths);

	for (i = 0; i < numberOfPaths; i++) {

		path = arrayGet(paths, i);
		numberOfNodes = arrayLength(path) - 1;

		for (j = 0; j < numberOfNodes; j++) {

			queuesAddNode(queues, (unsigned long) arrayGet(path, j), numberOfPaths);
		}
	}
}

void queuesFree(t_queues * queues) {

	unsigned long i;

	for (i = (unsigned long) listBegin(queues->activeNodes);
		i; i = (unsigned long) listNext(queues->activeNodes)) {

		listFreeWithData(queues->localQueue[i-1].packets);
		free(queues->localQueue[i-1].packets);
		arrayFree(queues->localQueue[i-1].individualFlowCounts);
		free(queues->localQueue[i-1].individualFlowCounts);
	}
	free(queues->localQueue);
	listFree(queues->activeNodes);
	free(queues->activeNodes);
}

int queuesSize(t_queues * queues, int node) {

	return(listLength(queues->localQueue[node].packets));
}

t_packet * queuesFirst(t_queues * queues, int node) {

	return(listBegin(queues->localQueue[node].packets));
}

void queuesAddPacket(t_queues * queues, t_packet * packet, int node) {

	t_packet * last;

	//printf("Queue Size: %ld\n",(unsigned long) arrayGet(queues->localQueue[node].individualFlowCounts, packet->flow));

//#ifdef OLD
	if ((unsigned long) arrayGet(queues->localQueue[node].individualFlowCounts, packet->flow) == queues->queueLimit) {

		for (last = listEnd(queues->localQueue[node].packets); last; last = listPrev(queues->localQueue[node].packets)) {

			if (last->flow == packet->flow) {
//printf("Discarding packet from flow %d at node %d due to overflow\n", last->flow, node);
				listDelCurrent(queues->localQueue[node].packets);
				free(last);
				break ;
			}
		}

//		listDelLastWithData(queues->packets[node]);
	}
	else {

		arrayInc(queues->localQueue[node].individualFlowCounts, packet->flow);
	}
//#endif
#ifdef DROPTAIL
	if (listLength(queues->localQueue[node].packets) == queues->queueLimit) {

		last = listEnd(queues->localQueue[node].packets);
		listDelCurrent(queues->localQueue[node].packets);
		free(last);
	}
#endif
	listAdd(queues->localQueue[node].packets, packet);
}

void queuesDelPacket(t_queues * queues, t_packet * packet, int node) {

	arrayDec(queues->localQueue[node].individualFlowCounts, packet->flow);
	listDel(queues->localQueue[node].packets, packet);
}

t_list * queuesNodeQueue(t_queues * queues, int node) {

	return(queues->localQueue[node].packets);
}

/*
 * TODO: this function assumes that:
 * 1) Only deterministic coding is possible, meaning only two packets can 
 * be coded together.
 * This condition can be overcome by inserting a heavier processing.
 */
t_packet * queuesFindCodingPartner(t_graph * graph, t_queues * queues, t_packet * packet, t_array * paths, int node,
									t_array * blockedLinks, t_array * priorityBlockedLinks, 
									int * linkIndexBase,
									double * successProb1,
									double * successProb2) {
return(NULL);
	t_packet * p;
	t_list * nodeQueue;
	unsigned long prevHopP1, prevHopP2, nextHopP1, nextHopP2;
//printf("Trying to find coding partner at node %d\n", node);
	if (packet->currentHop == 0) return(NULL);
//printf("Current hop is not 0. Continuing...\n");
	prevHopP1 = (unsigned long) arrayGet(arrayGet(paths, packet->flow), packet->currentHop - 1);
	nextHopP1 = (unsigned long) arrayGet(arrayGet(paths, packet->flow), packet->currentHop + 1);

	nodeQueue = queues->localQueue[node].packets;
	for (p = listBegin(nodeQueue); p; p = listNext(nodeQueue)) {
//printf("Evaluating packet %p\n", p);
		if (p == packet) continue ;

//printf("Not equal to current packet\n");
		if (p->currentHop == 0) continue ;
//printf("Not in its first hop\n");

		prevHopP2 = (unsigned long) arrayGet(arrayGet(paths, p->flow), p->currentHop - 1);
		if (prevHopP2 != nextHopP1)
			* successProb1 = sqrt((double) GRAPH_MULTIPLIER / (double) graphGetCost(graph, prevHopP2, nextHopP1));
		else
			* successProb1 = 1;
		if (* successProb1 < 0.8) continue ;
		//printf("Previous hop match 1\n");

		nextHopP2 = (unsigned long) arrayGet(arrayGet(paths, p->flow), p->currentHop + 1);
		if (prevHopP1 != nextHopP2)
			* successProb2 = sqrt((double) GRAPH_MULTIPLIER / (double) graphGetCost(graph, prevHopP1, nextHopP2));
		else
			* successProb2 = 1;
		if (* successProb2 < 0.8) continue ;

		if (arrayGet(blockedLinks, simulationConflictNodeIndex(linkIndexBase, p->flow, p->currentHop))) continue ;
//printf("Packet is not blocked\n");

		if (arrayGet(priorityBlockedLinks, simulationConflictNodeIndex(linkIndexBase, p->flow, p->currentHop))) continue ;
//printf("Packet is not priority blocked\n");

		return(p);
	}

	return(NULL);
}

t_graph * simulationConflictGraph(t_graph * graph, t_array * paths, int * linkIndexBase) {

	t_graph * conflict;
	t_array * path1, * path2;
	int numberOfLinks;
	int i, j, k, l;
	long head1, head2, tail1, tail2;

	numberOfLinks = 0;
	for (i = 0; i < arrayLength(paths); i++) {

		path1 = arrayGet(paths, i);
		linkIndexBase[i] = numberOfLinks;
		numberOfLinks += (arrayLength(path1) - 1);
	}

	conflict = graphNew(numberOfLinks);

	for (i = 0; i < arrayLength(paths); i++) {

		path1 = arrayGet(paths, i);
		for (j = 0; j < arrayLength(path1) - 1; j++) {

			head1 = (long) arrayGet(path1, j);
			tail1 = (long) arrayGet(path1, j+1);
			for (k = i; k < arrayLength(paths); k++) {

				path2 = arrayGet(paths, k);
				for (l = (k == i) ? j : 0; l < arrayLength(path2) - 1; l++) {

					head2 = (long) arrayGet(path2, l);
					tail2 = (long) arrayGet(path2, l+1);
#define CONFLICT_LIMIAR		(GRAPH_MULTIPLIER / 0.01)
//#define CONFLICT_LIMIAR		GRAPH_INFINITY
					if (graphGetCost(graph, head1, head2) < CONFLICT_LIMIAR) {
//					if (graphGetCost(graph, head1, head2) < GRAPH_INFINITY) {

						graphAddLink(conflict, 
							simulationConflictNodeIndex(linkIndexBase, i, j),
							simulationConflictNodeIndex(linkIndexBase, k, l),
							1);
						graphAddLink(conflict, 
							simulationConflictNodeIndex(linkIndexBase, k, l),
							simulationConflictNodeIndex(linkIndexBase, i, j),
							1);
					}
					else if (graphGetCost(graph, head2, head1) < CONFLICT_LIMIAR) {
//					else if (graphGetCost(graph, head2, head1) < GRAPH_INFINITY) {

						graphAddLink(conflict, 
							simulationConflictNodeIndex(linkIndexBase, i, j),
							simulationConflictNodeIndex(linkIndexBase, k, l),
							1);
						graphAddLink(conflict, 
							simulationConflictNodeIndex(linkIndexBase, k, l),
							simulationConflictNodeIndex(linkIndexBase, i, j),
							1);
					}
					else if (graphGetCost(graph, head2, tail1) < CONFLICT_LIMIAR) {
//					else if (graphGetCost(graph, head2, tail1) < GRAPH_INFINITY) {

						graphAddLink(conflict, 
							simulationConflictNodeIndex(linkIndexBase, i, j),
							simulationConflictNodeIndex(linkIndexBase, k, l),
							1);
						graphAddLink(conflict, 
							simulationConflictNodeIndex(linkIndexBase, k, l),
							simulationConflictNodeIndex(linkIndexBase, i, j),
							1);
					}
					else if (graphGetCost(graph, head1, tail2) < CONFLICT_LIMIAR) {
//					else if (graphGetCost(graph, head1, tail2) < GRAPH_INFINITY) {

						graphAddLink(conflict, 
							simulationConflictNodeIndex(linkIndexBase, i, j),
							simulationConflictNodeIndex(linkIndexBase, k, l),
							1);
						graphAddLink(conflict, 
							simulationConflictNodeIndex(linkIndexBase, k, l),
							simulationConflictNodeIndex(linkIndexBase, i, j),
							1);
					}
				}
			}
		}
	}

//printf("Conflict graph:\n\n");
//graphPrint(conflict);
	return(conflict);
}

//#define STATE_HASH_SIZE		1024
#define STATE_HASH_SIZE		(1 << 24)
//#define STATE_HASH_SIZE		8388608

void printPaths(t_array * paths) {

	t_array * path;
	int i, j;

	printf("Simulating with %d paths:\n", arrayLength(paths));
	for (i = 0; i < arrayLength(paths); i++) {

		path = arrayGet(paths, i);
		for (j = 0; j < arrayLength(path) - 1; j++) {
	
			printf("%ld -> ", (long) arrayGet(path, j));
		}
		printf("%ld\n", (long) arrayGet(path, j));
	}
}

t_return * simulationSimulate(t_graph * graph, t_array * paths, t_array * flowTimes, t_array * frameTxDurations) {

	int * linkIndexBase;
	t_graph * conflict;
	t_array * path;
	t_array * backoff;
	t_list * neighbors, * nodeQueue;
	t_list * waitingNodes, * onTransmissionPackets;
	t_array * blockedLinks, * priorityBlockedLinks, * priorityBlockedNodes;
	t_packet * newPacket, * packet, * codedPacket, * otherPacket;
	t_state * oldState, * state;
	t_stateStorage * stateStorage;
	t_array * deliveredPacketsFlows;
	t_weight time, oldTime, delta, oldDelta;
	int i, j, k;
	int * link;
	double deliveredPackets, oldDeliveredPackets;
	int slots;
	int numberOfFlows;
	long node, lastNode, node2;
	int * nodep;
	float meanInterval, oldMeanInterval, meanTime, meanDelivery;
	t_queues * queues;
	int lookAhead = 1;
	int maxDeliveredPerFlow = 100;
	float alfa = 0.8;
	double tmp;
	int maxFlowsPerNode = 0;
	t_array * flowsPerNode;
	int haveToSaveState;
	int numberOfStates = 1;
	//float slotTime = 0.00002; //20us
	float slotTime = 0.000009; //9us
	//float txTime = frameTxDuration; //12ms
	//float txTime = 0.012; //12ms
	t_weight * airTime;
	double * backoffUnit;
	unsigned char * numberOfRetries;
	int numberOfLinks = 0;
	double successProb1, successProb2;
	t_weight targetTime = GRAPH_INFINITY;
	t_list * activeNodes;
	t_array * scheduleFlowTime;
	int scheduleTime;
	t_array * idPacketFlows, * delayFlows;
	t_array * permanentDeliveredPacketsFlows, * permanentSentPacketsFlows;
	
	t_return * r;

	float * deliveredPacketsPerFlow, * oldDeliveredPacketsPerFlow, * meanDeliveryPerFlow, 
		  * meanIntervalPerFlow, * oldMeanIntervalPerFlow, * meanSentPerFlow, * oldSentPacketsPerFlow, * meanDelayFlows, * oldDelayFlows;


	numberOfFlows = arrayLength(paths);

	scheduleFlowTime = arrayNew(numberOfFlows);

	deliveredPacketsFlows = arrayNew(numberOfFlows);
	for (i = 0; i < numberOfFlows; i++) arraySet(deliveredPacketsFlows, i, 0);

	idPacketFlows = arrayNew(numberOfFlows);
	for (i = 0; i < numberOfFlows; i++) arraySet(idPacketFlows, i, 0);

	permanentDeliveredPacketsFlows = arrayNew(numberOfFlows);
	for (i = 0; i < numberOfFlows; i++) arraySet(permanentDeliveredPacketsFlows, i, 0);
	
	delayFlows = arrayNew(numberOfFlows);
	for (i = 0; i < numberOfFlows; i++) arraySet(delayFlows, i, 0);
	
	permanentSentPacketsFlows = arrayNew(numberOfFlows);
	for (i = 0; i < numberOfFlows; i++) arraySet(permanentSentPacketsFlows, i, 0);

	MALLOC(deliveredPacketsPerFlow, sizeof(float) * numberOfFlows);
	MALLOC(oldDeliveredPacketsPerFlow, sizeof(float) * numberOfFlows);
	MALLOC(meanDeliveryPerFlow, sizeof(float) * numberOfFlows);
	//MALLOC(meanIntervalPerFlow, sizeof(float) * numberOfFlows);
	MALLOC(oldMeanIntervalPerFlow, sizeof(float) * numberOfFlows);
	
	MALLOC(meanSentPerFlow, sizeof(float) * numberOfFlows);
	MALLOC(oldSentPacketsPerFlow, sizeof(float) * numberOfFlows);
	
	MALLOC(meanDelayFlows, sizeof(float) * numberOfFlows);
	MALLOC(oldDelayFlows, sizeof(float) * numberOfFlows);

	for (i = 0; i < numberOfFlows; i++){
		deliveredPacketsPerFlow[i] = 0.0f;
		oldDeliveredPacketsPerFlow[i] = 0.0f;
		meanDeliveryPerFlow[i] = 0.0f;
		//meanIntervalPerFlow[j] = 0.0f;
		oldMeanIntervalPerFlow[i] = 0.0f;
		meanSentPerFlow[i] = 0.0f;
		oldSentPacketsPerFlow[i] = 0.0f;
		meanDelayFlows[i] = 0.0f;
		oldDelayFlows[i] = 0.0f;
	}

	flowsPerNode = arrayNew(graphSize(graph));
	arrayClear(flowsPerNode);
	for (i = 0; i < numberOfFlows; i++) {
		//printf("Flow %d - FlowTime=%d FrameTxDuration=%d ms\n" , i, (int) arrayGet(flowTimes, i), (int) arrayGet(frameTxDurations, i));
		arraySet(scheduleFlowTime,i,arrayGet(flowTimes, i));

		node = (long) arrayGet(arrayGet(paths, i), 0);
		arrayInc(flowsPerNode, node);
		if ((long) arrayGet(flowsPerNode, node) > maxFlowsPerNode) maxFlowsPerNode = (long) arrayGet(flowsPerNode, node);

		numberOfLinks += (arrayLength(arrayGet(paths, i)) - 1);
	}

	arrayFree(flowsPerNode);
	free(flowsPerNode);

	MALLOC(airTime, sizeof(t_weight) * numberOfLinks);
	MALLOC(numberOfRetries, sizeof(unsigned char) * numberOfLinks);
	MALLOC(backoffUnit, sizeof(double) * numberOfLinks);
	k = 0;
	for (i = 0; i < numberOfFlows; i++) {

		for (j = 1; j < arrayLength(arrayGet(paths, i)); j++) {

			lastNode = (long) arrayGet(arrayGet(paths, i), j - 1);
			node = (long) arrayGet(arrayGet(paths, i), j);
			tmp = (double) GRAPH_MULTIPLIER / (double) graphGetCost(graph, lastNode, node);
			airTime[k] = GRAPH_MULTIPLIER * ((tmp * (1 + (1-tmp) * (2 + (1-tmp) * 3))) + 4 * (1-tmp) * (1-tmp) * (1-tmp));
			numberOfRetries[k] = round((double) airTime[k] / (double) GRAPH_MULTIPLIER);
			// backoffUnit[k] = ((tmp * (15.5 + (1-tmp) * (47 + (1-tmp) * 110.5))) + 238 * (1-tmp) * (1-tmp) * (1-tmp)); //802.11b
			backoffUnit[k] = ((tmp * (7.5 + (1-tmp) * (23 + (1-tmp) * 54.5))) + 118 * (1-tmp) * (1-tmp) * (1-tmp)); //802.11g
			backoffUnit[k] /= ((1 << numberOfRetries[k]) - 1);
			airTime[k] /= numberOfRetries[k];
//			airTime[k] = GRAPH_MULTIPLIER;
//printf("Airtime = %lu, numberOfRetries = %hhu, backoffUnit = %f, tmp = %f\n", airTime[k], numberOfRetries[k], backoffUnit[k], tmp);
			k++;
		}
	}

	backoff = arrayNew(graphSize(graph));
	arrayClear(backoff);

	/*
	 * Compute conflict graph for the input paths.
	 */
	MALLOC(linkIndexBase, sizeof(int) * numberOfFlows);
	conflict = simulationConflictGraph(graph, paths, linkIndexBase);
//graphPrint(conflict);
	/*
	 * Allocate queueing information.
	 */
	queues = queuesNew(graphSize(graph), 2 * maxFlowsPerNode);
	queuesAddPaths(queues, paths);
	activeNodes = queuesActiveNodes(queues);

	/*
	 * We'll keep an updated state of the links which 
	 * are currently blocked.
	 */
	blockedLinks = arrayNew(graphSize(conflict));
	priorityBlockedLinks = arrayNew(graphSize(conflict));
	priorityBlockedNodes = arrayNew(graphSize(graph));
	arrayClear(blockedLinks);

	/*
	 * We'll keep track of the states here.
	 */
	stateStorage = stateStorageNew(STATE_HASH_SIZE);
	slots = linkIndexBase[arrayLength(paths) - 1] + arrayLength(arrayGet(paths, arrayLength(paths) - 1)) - 1;

	/*
	 * We'll keep track of the time here.
	 */
	time = 0.0;

	/*
	 * We also keep track of 'delta', the interval
	 * until the next event. Since we don't know
	 * the value beforehand, we must computed.
	 */
	delta = GRAPH_INFINITY;

	/*
	 * Finally, we keep track of the number of
	 * delivered packets.
	 */
	deliveredPackets = 0;

	/*
	 * We'll have a list for nodes waiting to 
	 * transmit and a list for packets on transmission.
	 */
	waitingNodes = listNew();
	onTransmissionPackets = listNew();

	/*
	 * Build an initial state.
	 */
	state = stateNew(slots, numberOfFlows);

	/* Return */
	MALLOC(r, sizeof(t_return));
	MALLOC(r->meanIntervalPerFlow, sizeof(float) * numberOfFlows);
	MALLOC(r->packetsLossPerFlow, sizeof(float) * numberOfFlows);
	MALLOC(r->delayFlows, sizeof(float) * numberOfFlows);
	MALLOC(r->rateFlows, sizeof(float) * numberOfFlows);

	MALLOC(r->optimal, sizeof(t_optimal_cycle));
	MALLOC(r->optimal->packetDeliveredBetweenStates, sizeof(float) * numberOfFlows);
	MALLOC(r->optimal->packetSentBetweenStates, sizeof(float) * numberOfFlows);
	MALLOC(r->optimal->timeBetweenStates, sizeof(float) * numberOfFlows);
	/*
	 * Initialization: iterate through all flows and put
	 * their first packets either on transmission, on backoff
	 * or on buffer.
	 */
	for (i = 0; i < numberOfFlows; i++) {

		path = arrayGet(paths, i);

		MALLOC(newPacket, sizeof(t_packet));
		arrayInc(idPacketFlows, i);
		newPacket->id = arrayGet(idPacketFlows, i);
		newPacket->initialTime = time;
		newPacket->flow = i;
		newPacket->deliveryProbability = 1.0;
		newPacket->ETA = airTime[simulationConflictNodeIndex(linkIndexBase, i, 0)]; //adicionado com base no simularionh
				
		/*
		 * Is the necessary link blocked?
		 */
		if (arrayGet(blockedLinks, simulationConflictNodeIndex(linkIndexBase, i, 0))) {

			/*
			 * Yes, packet stays in hop 0.
			 */
			newPacket->currentHop = 0;

			/*
			 * In this first set of transmissions, we do not simulate
			 * the backoff because all nodes would have their backoff
			 * couters at the same time. Hence, we skip this useless
			 * step and go ahead transmitting packets. Nevertheless,
			 * a node may not be able to transmit a packet right away
			 * because of another previously scheduled conflicting packet.
			 * In this case, the packet to be transmitted must be kept 
			 * on the transmitters buffer for another opportunity. There
			 * are two distinct scenarios that can lead to this situation:
			 * In the most probable scenario, we'll keep this blocked packet 
			 * in the backoff buffer of the node (i.e., we assume it was in the
			 * backoff buffer, the backoff counter has reached zero and now 
			 * the node must wait for an opportunity to transmit). However, 
			 * in case of multiple flows rooted at the same node, this spot (the 
			 * backoff buffer) might already be occupied. In this case, 
			 * we leave the packet at the node's regular buffer.
			 */
			if (arrayGet(backoff, (long) arrayGet(path, 0))) {

				/*
				 * Case 1: backoff buffer is already taken.
				 * Add the packet to the node's queue.
				 */
				queuesAddPacket(queues, newPacket, (long) arrayGet(path, 0));

				/*
				 * Add the information about this packet staying on the
				 * buffer to the current state.
				 */
				stateAddBuffer(state, simulationConflictNodeIndex(linkIndexBase, i, newPacket->currentHop));
			}
			else {

				newPacket->retries = 0;
				newPacket->maxRetries = numberOfRetries[simulationConflictNodeIndex(linkIndexBase, newPacket->flow, newPacket->currentHop)];
				newPacket->ETA = 0; // First backoff is already done.
				newPacket->waitingSince = 0;
				arraySet(backoff, (long) arrayGet(path, 0), newPacket);
				listAdd(waitingNodes, (void *) (((long) arrayGet(path, 0)) + 1));

				stateAddTransmission(state, simulationConflictNodeIndex(linkIndexBase, i, 0), newPacket->ETA, 1, newPacket->retries, 0);
			}
		}
		else {

			/*
			 * No, it is transmitted. We code that
			 * by saying the packet is in the -1 hop.
			 */
			newPacket->currentHop = -1;
//printf("Acessing pos %d with value %lu\n", simulationConflictNodeIndex(linkIndexBase, i, 0), airTime[simulationConflictNodeIndex(linkIndexBase, i, 0)]);
			newPacket->ETA = airTime[simulationConflictNodeIndex(linkIndexBase, i, 0)];
			tmp = 1 - sqrt((double) GRAPH_MULTIPLIER / (double) graphGetCost(graph, (long) arrayGet(path, 0), (long) arrayGet(path, 1)));
			newPacket->deliveryProbability *= (1 - tmp * tmp * tmp * tmp);

			/*
			 * Fill also information regarding the backoff.
			 */
			newPacket->retries = 0;
			newPacket->maxRetries = numberOfRetries[simulationConflictNodeIndex(linkIndexBase, i, 0)];
			arraySet(backoff, (long) arrayGet(path, 0), newPacket);

			/*
			 * Update delta, if necessary.
			 */
			if (delta > newPacket->ETA) {

				delta = newPacket->ETA;
			}

			/*
			 * Block links.
			 */
			neighbors = graphGetNeighbors(conflict, simulationConflictNodeIndex(linkIndexBase, i, 0));
			for (link = listBegin(neighbors); link; link = listNext(neighbors)) {

				arrayInc(blockedLinks, * link);
			}

			/*
			 * Add the transmission information to the current state.
			 */
			stateAddTransmission(state, simulationConflictNodeIndex(linkIndexBase, i, 0), newPacket->ETA, 0, newPacket->retries, 0);

			/*
			 * Also add it to the onTransmissionPackets list,
			 * so that we know this packet is disputing
			 * the wireless medium.
			 */
			listAdd(onTransmissionPackets, newPacket);

			//Cannot generate another packet imediatelly, since the next packet will arrive only at the next Flow Time.
			///*
			// * We'll generate another packet for this flow. 
			// */
			//
			//MALLOC(newPacket, sizeof(t_packet));
			//arrayInc(idPacketFlows, i);
			//newPacket->id = arrayGet(idPacketFlows, i);
			//newPacket->initialTime = time;
			//newPacket->currentHop = 0;
			//newPacket->flow = i;
			//newPacket->deliveryProbability = 1.0;
			//
			///*
			// * Place it on the ordinary buffer.
			// */
//
			//queuesAddPacket(queues, newPacket, (long) arrayGet(arrayGet(paths, newPacket->flow), 0));
			//stateAddBuffer(state, simulationConflictNodeIndex(linkIndexBase, newPacket->flow, newPacket->currentHop));
			
		}
	}

	stateSetCurrentTime(state, 0.0);
	stateSetDeliveredPackets(state, 0);
	stateSetDeliveredPacketsFlows(state, permanentDeliveredPacketsFlows);
	stateSetSentPacketsFlows(state, permanentSentPacketsFlows);
	stateSetDelayFlows(state, delayFlows);
//printf("At " WEIGHT_FORMAT ":", time);
//statePrint(state);
	stateLookupAndStore(state, stateStorage);

	/*
	 * Main loop:
	 * 0) Update backoff counters.
	 * 1) Loop through the packets of the onTransmissionPackets,
	 * updating the remaining transmission time and updating delta.
	 * If the new ETA is 0, handle the packet arrival.
	 * 2) Loop through the waitingNodes list trying to 
	 * transmit each packet and (possibly) updating delta.
	 */
	while(1) {

		haveToSaveState = 0;

		/*
		 * Clear some variables.
		 */
		state = stateNew(slots, numberOfFlows);
		arrayClear(priorityBlockedLinks);
		arrayClear(priorityBlockedNodes);

		/*
		 * Update time variables.
		 */
		time += delta;
		oldDelta = delta;
		delta = GRAPH_INFINITY;
		// printf("Simulation time: %ld\n",time);
		/*
		 * Update backoffs.
		 */
		for (node = (long) listBegin(activeNodes); node; node = (long) listNext(activeNodes)) {

			node--;

			if ((packet = arrayGet(backoff, node)) == NULL) continue ;

			/*
			 * The packet in the backoff buffer can be under transmission.
			 * We ignore those.
			 */
			if (packet->currentHop < 0) continue ;

			if (packet->ETA <= 0) continue ;

			/*
			 * See if any of the ongoing transmissions affects 
			 * the backoff counter (prevents it from decreasing).
			 */

			for (otherPacket = listBegin(onTransmissionPackets); otherPacket; otherPacket = listNext(onTransmissionPackets)) {

				node2 = (long) arrayGet(arrayGet(paths, otherPacket->flow), -otherPacket->currentHop - 1);

				// printf(">> GRAPH COST: %d, CONFLICT LIMIAR: %d", graphGetCost(graph, node, node2), CONFLICT_LIMIAR);
				
				if (graphGetCost(graph, node, node2) < CONFLICT_LIMIAR) break ;
			}

			if (!otherPacket) {

				/*
				 * No neighbor was transmitting.
				 * Backoff counter is decreased.
				 */

				packet->ETA -= oldDelta;
			}
		}

		/*
		 * Update status of the packets being transmitted.
		 */

		for (packet = listBegin(onTransmissionPackets); packet; packet = listNext(onTransmissionPackets)) {

			packet->ETA -= oldDelta;

			if (packet->ETA <= 0) {

				/*
				 * This packet has just arrived at its
				 * next hop. Fix the fields.
				 */
				packet->currentHop *= -1;

// printf("At time %ld, Packet just arrived at hop %d for flow %d with probability %f\n", time, packet->currentHop, packet->flow, packet->deliveryProbability);
				/*
				 * Unblock (or at least decrease its
				 * contribution) the links previous
				 * blocked.
				 */
				neighbors = graphGetNeighbors(conflict, simulationConflictNodeIndex(linkIndexBase, packet->flow, packet->currentHop - 1));
				for (link = listBegin(neighbors); link; link = listNext(neighbors)) {

					arrayDec(blockedLinks, * link);
				}

				/*
				 * Remove the packet from the onTransmissionPackets list.
				 */
				listDelCurrent(onTransmissionPackets);

				/*
				 * But did the packet really arrive, or do we need more
				 * retransmissions?
				 */
				if (++packet->retries < packet->maxRetries) {

					/*
					 * We need more retries. Put the packet 
					 * back on the source node's backoff buffer.
					 * We do this by decreasing currentHop and
					 * reseting the backoff counter.
					 */

					packet->currentHop--;
//					packet->ETA = (((32 * (1 << packet->retries) - 1) / 2.0) * slotTime / txTime) * GRAPH_MULTIPLIER;
					packet->ETA = ((backoffUnit[simulationConflictNodeIndex(linkIndexBase, packet->flow, packet->currentHop)] * 
							(1 << packet->retries)) * slotTime / ((int) arrayGet(frameTxDurations,packet->flow)/1000000.0)) * GRAPH_MULTIPLIER;
					packet->waitingSince = time;

					listAdd(waitingNodes, (void *) (((long) arrayGet(arrayGet(paths, packet->flow), packet->currentHop)) + 1));

					stateAddTransmission(state, simulationConflictNodeIndex(linkIndexBase, packet->flow, packet->currentHop), packet->ETA, 1, packet->retries, time - packet->waitingSince);
				}
				else {

					/*
					 * This transmission is definetly done. We have
					 * to place the next packet on the backoff buffer 
					 * of the source node (if one exists).
					 */

					node = (long) arrayGet(arrayGet(paths, packet->flow), packet->currentHop - 1);
					if (queuesSize(queues, node) > 0) {

						otherPacket = queuesFirst(queues, node);
						queuesDelPacket(queues, otherPacket, node);
						arraySet(backoff, node, otherPacket);

//						otherPacket->ETA = 0.025833333 * GRAPH_MULTIPLIER; // First backoff: 15.5 slots.
						otherPacket->ETA = (backoffUnit[simulationConflictNodeIndex(linkIndexBase, otherPacket->flow, otherPacket->currentHop)] * 
								slotTime / ((int) arrayGet(frameTxDurations,otherPacket->flow)/1000000.0)) * GRAPH_MULTIPLIER; // First backoff.
						otherPacket->waitingSince = time;
						otherPacket->retries = 0;
						otherPacket->maxRetries = numberOfRetries[simulationConflictNodeIndex(linkIndexBase, otherPacket->flow, otherPacket->currentHop)];
						listAdd(waitingNodes, (void *) (((long) arrayGet(arrayGet(paths, packet->flow), packet->currentHop - 1)) + 1));

						stateAddTransmission(state, simulationConflictNodeIndex(linkIndexBase, otherPacket->flow, otherPacket->currentHop), otherPacket->ETA, 1, otherPacket->retries, time - otherPacket->waitingSince);
					}
					else {

						arraySet(backoff, node, NULL);
					}

					/*
					 * Has the packet arrived in its final
					 * destination?
					 */
					if (packet->currentHop == arrayLength(arrayGet(paths, packet->flow)) - 1) {

						/*
						 * Yes. Update the number of delivered packets and
						 * free the packet.
						 */
						//arrayInc(permanentDeliveredPacketsFlows, packet->flow);
						arraySet(permanentDeliveredPacketsFlows, packet->flow, ((long) (packet->deliveryProbability * GRAPH_MULTIPLIER)) + ((long) arrayGet(permanentDeliveredPacketsFlows, packet->flow)));
						//printf("Packet of Flow %d delivered with probability %f. (Total Delivered:%f)\n", packet->flow, packet->deliveryProbability, ((long) arrayGet(permanentDeliveredPacketsFlows, packet->flow)) / GRAPH_MULTIPLIER);

						arraySet(permanentSentPacketsFlows, packet->flow, packet->id);
						// printf("Packet of Flow %d sent. (Total Sent:%d)\n",packet->flow, arrayGet(permanentSentPacketsFlows, packet->flow));
						//printf("DELAY FLOW %d: %ld",packet->flow, (long) arrayGet(delayFlows, packet->flow));

						arraySet(delayFlows, packet->flow, (long) arrayGet(delayFlows, packet->flow) + (time - packet->initialTime));
						
						//printf("DELAY FLOW AFTER %d:  %ld",packet->flow, (long) arrayGet(delayFlows, packet->flow));

						arrayInc(deliveredPacketsFlows, packet->flow);
						deliveredPackets += packet->deliveryProbability;

						deliveredPacketsPerFlow[packet->flow] = packet->deliveryProbability + deliveredPacketsPerFlow[packet->flow];
// printf("Packet just delivered for flow %d: %f vs. %f\n", packet->flow, packet->deliveryProbability, deliveredPackets);
				//		printf("Packet just delivered for flow %d: %f vs. %f\n", packet->flow, packet->deliveryProbability, deliveredPacketsPerFlow[packet->flow]);
						free(packet);
					}
					else {

						/*
						 * Add the packet to the node's queue. Then, if the node
						 * is not currently performing a backoff, take the
						 * first packet on the queue and start the backoff procedure. 
						 */
						queuesAddPacket(queues, packet, (long) arrayGet(arrayGet(paths, packet->flow), packet->currentHop));
						if (!arrayGet(backoff, (long) arrayGet(arrayGet(paths, packet->flow), packet->currentHop))) {

							otherPacket = queuesFirst(queues, (long) arrayGet(arrayGet(paths, packet->flow), packet->currentHop));
							queuesDelPacket(queues, otherPacket, (long) arrayGet(arrayGet(paths, packet->flow), packet->currentHop));
							arraySet(backoff, (long) arrayGet(arrayGet(paths, packet->flow), packet->currentHop), otherPacket);

							/* 
							 * Fill data regarding backoff.
							 */
//							otherPacket->ETA = 0.025833333 * GRAPH_MULTIPLIER; // First backoff: 15.5 slots.
							otherPacket->ETA = (backoffUnit[simulationConflictNodeIndex(linkIndexBase, packet->flow, packet->currentHop)] 
									* slotTime / ((int) arrayGet(frameTxDurations,packet->flow)/1000000.0)) * GRAPH_MULTIPLIER; // First backoff.
							otherPacket->waitingSince = time;
							otherPacket->retries = 0;
							otherPacket->maxRetries = numberOfRetries[simulationConflictNodeIndex(linkIndexBase, otherPacket->flow, otherPacket->currentHop)];

							listAdd(waitingNodes, (void *) (((long) arrayGet(arrayGet(paths, packet->flow), packet->currentHop)) + 1));
							stateAddTransmission(state, simulationConflictNodeIndex(linkIndexBase, otherPacket->flow, otherPacket->currentHop), otherPacket->ETA, 1, otherPacket->retries, time - otherPacket->waitingSince);
						}
					}
				}
			}
			else {

				/*
				 * This packet continues to be transmited.
				 * Update delta, if necessary.
				 */
				if (delta > packet->ETA) {

					delta = packet->ETA;
				}

				stateAddTransmission(state, simulationConflictNodeIndex(linkIndexBase, packet->flow, -packet->currentHop - 1), packet->ETA, 0, packet->retries, time - packet->waitingSince);
			}
		}


		/* Schedule - NOVO BLOCO PARA RAJADAS DE PACOTES */

        // --- INÍCIO DA CONFIGURAÇÃO DO PADRÃO DE RAJADAS ---

        // Defina o pequeno intervalo entre pacotes DENTRO de uma mesma rajada.
        // Um valor pequeno para simular envio "back-to-back". Ex: 10 microssegundos.
        const int short_inter_packet_delay = 0;

        // Padrão de tráfego gerado a partir do trace 'st_alta.txt'.
		// Pressupostos: Pacote de 1024 bytes, 1 segundo = 18189167 unidades de tempo.
		t_burst_step trace_pattern_alta[] = {
			{ .num_packets_in_burst = 34, .inter_burst_interval = 2167109 },	// Intervalo até o próximo frame
			{ .num_packets_in_burst = 5, .inter_burst_interval = 18189 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 5197 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 2143723 },
			{ .num_packets_in_burst = 5, .inter_burst_interval = 18189 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 23386 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 1896870 },
			{ .num_packets_in_burst = 5, .inter_burst_interval = 246853 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 18189 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 1384975 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 62363 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 660007 },
			{ .num_packets_in_burst = 32, .inter_burst_interval = 2060573 },
			{ .num_packets_in_burst = 5, .inter_burst_interval = 62363 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 345594 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 1766948 },
			{ .num_packets_in_burst = 5, .inter_burst_interval = 64961 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 314413 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 1785137 },
			{ .num_packets_in_burst = 5, .inter_burst_interval = 72757 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 288428 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 1158910 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 54568 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 693787 },
			{ .num_packets_in_burst = 35, .inter_burst_interval = 2161912 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 59764 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 41575 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 2089156 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 106537 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 38977 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 1987816 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 150710 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 72757 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 1190091 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 181892 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 535281 },
			{ .num_packets_in_burst = 33, .inter_burst_interval = 2034588 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 275436 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 166301 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 1681199 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 441737 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 54568 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 1621434 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 595046 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 62363 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 987412 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 155907 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 571660 },
			{ .num_packets_in_burst = 33, .inter_burst_interval = 2156716 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 166301 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 59764 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 1935847 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 285830 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 122127 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 1748758 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 363783 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 20788 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 1117335 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 376776 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 410555 },
			{ .num_packets_in_burst = 33, .inter_burst_interval = 2039785 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 327405 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 77954 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 1748758 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 283231 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 213073 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 1665608 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 366382 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 12992 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 1060169 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 85749 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 787331 },
			{ .num_packets_in_burst = 32, .inter_burst_interval = 2011202 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 317011 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 166301 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 1681199 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 324807 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 257247 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 1572064 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 262444 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 176695 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 899065 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 452131 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 724968 },
			{ .num_packets_in_burst = 32, .inter_burst_interval = 1808523 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 371579 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 275436 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 1408361 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 569061 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 216105 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 1485449 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 566896 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 127324 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 745756 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 463824 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 257680 },
			{ .num_packets_in_burst = 33, .inter_burst_interval = 2155416 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 724535 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 21221 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 1540016 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 503234 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 139450 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 1521827 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 385004 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 269806 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 657842 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 266774 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 527486 },
			{ .num_packets_in_burst = 34, .inter_burst_interval = 2091754 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 403193 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 278901 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 1479386 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 421382 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 275869 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 1458165 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 715441 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 18189 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 854891 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 433508 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 309216 },
			{ .num_packets_in_burst = 33, .inter_burst_interval = 1997777 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 742724 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 166734 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 1242926 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 466855 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 315279 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 1382377 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 475950 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 321342 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 642684 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 482013 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 497171 },
			{ .num_packets_in_burst = 34, .inter_burst_interval = 1888642 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 509297 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 339531 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 1491512 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 345594 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 336500 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 1482417 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 860954 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 18189 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 385004 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 533549 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 533549 },
			{ .num_packets_in_burst = 33, .inter_burst_interval = 1997777 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 721504 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 18189 },
			{ .num_packets_in_burst = 1, .inter_burst_interval = 1242926 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 190986 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 726404 },	// Intervalo médio para looping
		};
		
		// Padrão de tráfego gerado a partir do trace 'st_media.txt'.
		// Total de 150 etapas.
		// Pressupostos: Pacote de 1024 bytes, 1 segundo = 18189167 unidades de tempo.
		t_burst_step trace_pattern_media[] = {
			{ .num_packets_in_burst = 19, .inter_burst_interval = 1800728 },	// Intervalo até o próximo frame
			{ .num_packets_in_burst = 5, .inter_burst_interval = 87308 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 794867 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 1145918 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 381973 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 429264 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1355093 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 603880 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 231002 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 894907 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 251011 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 665724 },
			{ .num_packets_in_burst = 23, .inter_burst_interval = 1811641 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 327405 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 192805 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1567906 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 405618 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 478375 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1315077 },
			{ .num_packets_in_burst = 5, .inter_burst_interval = 261924 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 383791 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1162288 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 129143 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 623888 },
			{ .num_packets_in_burst = 22, .inter_burst_interval = 1733428 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 225546 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 652991 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 1320534 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 363783 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 511116 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1324171 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 296483 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 436540 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 904002 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 274656 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 580234 },
			{ .num_packets_in_burst = 22, .inter_burst_interval = 1933508 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 280113 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 378335 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1473323 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 378335 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 403800 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1204123 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 603880 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 472918 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 762126 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 212813 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 576597 },
			{ .num_packets_in_burst = 21, .inter_burst_interval = 2057195 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 336500 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 125505 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1447858 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 452910 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 449272 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1195028 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 574778 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 187348 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1002223 },
			{ .num_packets_in_burst = 5, .inter_burst_interval = 283751 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 585691 },
			{ .num_packets_in_burst = 21, .inter_burst_interval = 2069927 },
			{ .num_packets_in_burst = 8, .inter_burst_interval = 374697 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 61843 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 1753436 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 154608 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 320129 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1398747 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 378335 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 558407 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 725748 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 307397 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 451091 },
			{ .num_packets_in_burst = 21, .inter_burst_interval = 1800728 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 622070 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 371059 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1285974 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 518391 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 351051 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1220493 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 458367 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 658448 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 762126 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 190986 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 407437 },
			{ .num_packets_in_burst = 21, .inter_burst_interval = 2009903 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 391067 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 361964 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1271423 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 340137 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 643897 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 1309620 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 421989 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 436540 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 729386 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 425627 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 381973 },
			{ .num_packets_in_burst = 21, .inter_burst_interval = 2082660 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 294665 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 220089 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 1498787 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 740299 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 163703 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 1258690 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 527486 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 311035 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 687551 },
			{ .num_packets_in_burst = 5, .inter_burst_interval = 469281 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 525667 },
			{ .num_packets_in_burst = 21, .inter_burst_interval = 1960792 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 334681 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 371059 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1384196 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 803961 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 232821 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1004042 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 558407 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 360146 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 742118 },
			{ .num_packets_in_burst = 5, .inter_burst_interval = 336500 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 500202 },
			{ .num_packets_in_burst = 21, .inter_burst_interval = 1829830 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 656629 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 478375 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 1271423 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 360146 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 436540 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1256871 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 572959 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 389248 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 456548 },
			{ .num_packets_in_burst = 5, .inter_burst_interval = 651172 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 378335 },
			{ .num_packets_in_burst = 21, .inter_burst_interval = 1757074 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 793048 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 327405 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 953112 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 578416 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 842158 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 1085893 },
			{ .num_packets_in_burst = 5, .inter_burst_interval = 285570 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 656629 },
			{ .num_packets_in_burst = 5, .inter_burst_interval = 629345 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 420170 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 405618 },
			{ .num_packets_in_burst = 21, .inter_burst_interval = 1808003 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 723929 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 231002 },
			{ .num_packets_in_burst = 2, .inter_burst_interval = 1000404 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 836702 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 729325 },	// Intervalo médio para looping
		};

		// Padrão de tráfego gerado a partir do trace 'st_baixa.txt'.
		// Total de 150 etapas.
		// Pressupostos: Pacote de 1024 bytes, 1 segundo = 18189167 unidades de tempo.
		t_burst_step trace_pattern_baixa[] = {
			{ .num_packets_in_burst = 14, .inter_burst_interval = 1798454 },	// Intervalo até o próximo frame
			{ .num_packets_in_burst = 8, .inter_burst_interval = 379699 },
			{ .num_packets_in_burst = 5, .inter_burst_interval = 102314 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1509701 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 643442 },
			{ .num_packets_in_burst = 5, .inter_burst_interval = 109135 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1605194 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 370604 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 113682 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1118634 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 215996 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 698009 },
			{ .num_packets_in_burst = 16, .inter_burst_interval = 1957609 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 140966 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 131871 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1709782 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 445635 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 113682 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1625657 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 413804 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 115956 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1130002 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 204628 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 659357 },
			{ .num_packets_in_burst = 16, .inter_burst_interval = 1707508 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 306942 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 620705 },
			{ .num_packets_in_burst = 5, .inter_burst_interval = 1323262 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 213723 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 629800 },
			{ .num_packets_in_burst = 5, .inter_burst_interval = 1384650 },
			{ .num_packets_in_burst = 6, .inter_burst_interval = 188713 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 513844 },
			{ .num_packets_in_burst = 5, .inter_burst_interval = 857164 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 341047 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 532033 },
			{ .num_packets_in_burst = 15, .inter_burst_interval = 2050829 },
			{ .num_packets_in_burst = 8, .inter_burst_interval = 236459 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 140966 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1698413 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 450993 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 822151 },
			{ .num_packets_in_burst = 5, .inter_burst_interval = 1553538 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 290702 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 102314 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1276707 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 320984 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 598822 },
			{ .num_packets_in_burst = 15, .inter_burst_interval = 1756515 },
			{ .num_packets_in_burst = 8, .inter_burst_interval = 326768 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 217815 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1458804 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 406529 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 151607 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1599837 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 299388 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 111863 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1178198 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 197808 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 696191 },
			{ .num_packets_in_burst = 14, .inter_burst_interval = 1731671 },
			{ .num_packets_in_burst = 8, .inter_burst_interval = 312270 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 110045 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1683401 },
			{ .num_packets_in_burst = 8, .inter_burst_interval = 317633 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 227006 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1642340 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 310452 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 122765 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1256233 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 135422 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 780612 },
			{ .num_packets_in_burst = 13, .inter_burst_interval = 1696594 },
			{ .num_packets_in_burst = 8, .inter_burst_interval = 450993 },
			{ .num_packets_in_burst = 5, .inter_burst_interval = 602120 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 1293390 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 345594 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 127209 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1432454 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 295751 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 148197 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1183561 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 356963 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 673551 },
			{ .num_packets_in_burst = 13, .inter_burst_interval = 1913166 },
			{ .num_packets_in_burst = 8, .inter_burst_interval = 293932 },
			{ .num_packets_in_burst = 5, .inter_burst_interval = 404710 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 1321443 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 413804 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 524732 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 1297027 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 343216 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 122765 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1019504 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 348683 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 548716 },
			{ .num_packets_in_burst = 13, .inter_burst_interval = 1756515 },
			{ .num_packets_in_burst = 8, .inter_burst_interval = 379699 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 100495 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1532455 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 458223 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 122765 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1572064 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 330132 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 98676 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1079366 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 142784 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 787841 },
			{ .num_packets_in_burst = 14, .inter_burst_interval = 1718876 },
			{ .num_packets_in_burst = 8, .inter_burst_interval = 376061 },
			{ .num_packets_in_burst = 5, .inter_burst_interval = 125633 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1500606 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 422896 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 109135 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1583155 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 312270 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 129143 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1261596 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 117774 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 708914 },
			{ .num_packets_in_burst = 13, .inter_burst_interval = 1667426 },
			{ .num_packets_in_burst = 8, .inter_burst_interval = 600302 },
			{ .num_packets_in_burst = 5, .inter_burst_interval = 93121 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1523360 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 345594 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 94940 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1515064 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 330132 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 227006 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 1265233 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 295751 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 615342 },
			{ .num_packets_in_burst = 13, .inter_burst_interval = 1754696 },
			{ .num_packets_in_burst = 8, .inter_burst_interval = 325859 },
			{ .num_packets_in_burst = 5, .inter_burst_interval = 310452 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 1436092 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 270724 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 2727 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1548234 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 270724 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 306942 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 1323262 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 142784 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 36367 },
			{ .num_packets_in_burst = 12, .inter_burst_interval = 1427840 },
			{ .num_packets_in_burst = 8, .inter_burst_interval = 72757 },
			{ .num_packets_in_burst = 5, .inter_burst_interval = 673551 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 1439730 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 144602 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 181892 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 1458804 },
			{ .num_packets_in_burst = 7, .inter_burst_interval = 524732 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 5637 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 1003135 },
			{ .num_packets_in_burst = 4, .inter_burst_interval = 109135 },
			{ .num_packets_in_burst = 3, .inter_burst_interval = 530214 },
			{ .num_packets_in_burst = 12, .inter_burst_interval = 1098650 },	// Intervalo médio para looping
		};
        // Agrupadores para facilitar o acesso no loop
        t_burst_step* burst_patterns[] = { trace_pattern_alta, trace_pattern_alta, trace_pattern_alta, trace_pattern_alta };
        int patterns_num_steps[] = { 150, 150, 150,150 }; // Quantidade de passos definidos em t_burst_step em cada padrão

        // --- FIM DA CONFIGURAÇÃO ---


        // --- GERENCIAMENTO DE ESTADO DA RAJADA ---
        // Variáveis estáticas para guardar o estado de cada fluxo entre os passos da simulação.
        static int flow_pattern_index[128];      // Qual passo do padrão o fluxo está (ex: a rajada de 5 ou de 3 pacotes).
        static int packets_left_in_burst[128];   // Quantos pacotes ainda faltam para enviar na rajada atual.
        static bool is_burst_logic_initialized = false;

        // Inicializa o estado dos fluxos na primeira vez que o schedule é executado.
        if (!is_burst_logic_initialized) {
            for (int k = 0; k < numberOfFlows; k++) {
                flow_pattern_index[k] = 0;
                // Começa com o número de pacotes da primeira rajada do padrão.
                packets_left_in_burst[k] = burst_patterns[k][0].num_packets_in_burst;
            }
            is_burst_logic_initialized = true;
        }
        // --- FIM DO GERENCIAMENTO DE ESTADO ---


        for (i = 0; i < numberOfFlows; i++) {
            scheduleTime = (int) arrayGet(scheduleFlowTime, i) - (int) oldDelta;

            if (scheduleTime <= 0) {
                // O temporizador expirou. HORA DE ENVIAR UM PACOTE.
                
                // 1. Cria e enfileira um pacote, como antes.
                MALLOC(newPacket, sizeof(t_packet));
                arrayInc(idPacketFlows, i);
                newPacket->id = arrayGet(idPacketFlows, i);
                newPacket->initialTime = time;
                newPacket->currentHop = 0;
                newPacket->flow = i;
                newPacket->deliveryProbability = 1.0;
                newPacket->ETA = airTime[simulationConflictNodeIndex(linkIndexBase, i, 0)];
                
                queuesAddPacket(queues, newPacket, (long) arrayGet(arrayGet(paths, newPacket->flow), 0));
                
                // Lógica de backoff (copiada do original)
                if (!arrayGet(backoff, (long) arrayGet(arrayGet(paths, newPacket->flow), 0))) {
                    otherPacket = queuesFirst(queues, (long) arrayGet(arrayGet(paths, newPacket->flow), 0));
                    queuesDelPacket(queues, otherPacket, (long) arrayGet(arrayGet(paths, newPacket->flow), 0));
                    arraySet(backoff, (long) arrayGet(arrayGet(paths, newPacket->flow), 0), otherPacket);
                    otherPacket->ETA = (backoffUnit[simulationConflictNodeIndex(linkIndexBase, newPacket->flow, newPacket->currentHop)] * slotTime / ((int) arrayGet(frameTxDurations, newPacket->flow)/1000000.0)) * GRAPH_MULTIPLIER;
                    otherPacket->waitingSince = time;
                    otherPacket->retries = 0;
                    otherPacket->maxRetries = numberOfRetries[simulationConflictNodeIndex(linkIndexBase, otherPacket->flow, otherPacket->currentHop)];
                    listAdd(waitingNodes, (void *) (((long) arrayGet(arrayGet(paths, newPacket->flow), newPacket->currentHop)) + 1));
                    stateAddTransmission(state, simulationConflictNodeIndex(linkIndexBase, otherPacket->flow, otherPacket->currentHop), otherPacket->ETA, 1, otherPacket->retries, time - otherPacket->waitingSince);
                }

                // 2. Decrementa o contador de pacotes da rajada atual.
                packets_left_in_burst[i]--;

                // 3. Decide qual será o próximo intervalo de tempo.
                int next_interval;
                if (packets_left_in_burst[i] > 0) {
                    // AINDA HÁ PACOTES NA RAJADA: usa o intervalo curto.
                    next_interval = short_inter_packet_delay;
                } else {
                    // A RAJADA ATUAL TERMINOU: inicia o período de silêncio.
                    
                    // Pega o intervalo de espera do passo atual do padrão.
                    next_interval = burst_patterns[i][flow_pattern_index[i]].inter_burst_interval;
                    
                    // Avança para o próximo passo do padrão (ex: da rajada de 5 para a de 3).
                    // Se chegar ao fim, volta ao início (loop).
                    flow_pattern_index[i] = (flow_pattern_index[i] + 1) % patterns_num_steps[i];
                    
                    // Reinicia o contador de pacotes para a PRÓXIMA rajada.
                    packets_left_in_burst[i] = burst_patterns[i][flow_pattern_index[i]].num_packets_in_burst;
                }
                
                // 4. Reconfigura o cronômetro com o próximo intervalo decidido.
                arraySet(scheduleFlowTime, i, (void*)(long)next_interval);

            } else {
                // Se o tempo não zerou, apenas atualiza o cronômetro.
                arraySet(scheduleFlowTime, i, (void*)(long)scheduleTime);
            }

            // Atualiza o delta global da simulação se o cronômetro deste fluxo for o menor.
            if (delta > (long)arrayGet(scheduleFlowTime, i)) {
                delta = (long)arrayGet(scheduleFlowTime, i);
            }
        }

		/*
		 * Now we loop through the list of the waiting nodes
		 * to see if we can transmit their packets.
		 */
		for (node = (long) listBegin(waitingNodes); node; node = (long) listNext(waitingNodes)) {

			/*
			 * Node is stored as node + 1 to avoid NULL values.
			 */
			node--;
			//printf("Node %d\n", node);
			/* 
			 * Always take the packet on the node's backoff buffer.
			 */
			//packet = arrayGet(backoff, node);
			if ((packet = arrayGet(backoff, node)) == NULL) continue ;
			

			/*
			 * Is the packet still in backoff mode or ready to be
			 * transmitted?
			 */
			
			if (packet->ETA > 0) {

				/* 
				 * Regardless of anything, we'll store into
				 * the state information that this transmition
				 * is ongoing (as a backoff).
				 */
				stateAddTransmission(state, simulationConflictNodeIndex(linkIndexBase, packet->flow, packet->currentHop), packet->ETA, 1, packet->retries, time - packet->waitingSince);

				/*
				 * Backoff is not over yet.
				 * At this point, we just have to 
				 * make sure the packet is not gonna suffer
				 * starvation. To this end, we check if there
				 * are nodes interfering with its backoff right 
				 * now. If there are, we do nothing. But if
				 * there are not, we place a priority block on
				 * every link rooted at neighbors of this node.
				 */
				for (otherPacket = listBegin(onTransmissionPackets); otherPacket; otherPacket = listNext(onTransmissionPackets)) {

					node2 = (long) arrayGet(arrayGet(paths, otherPacket->flow), -otherPacket->currentHop - 1);

					if (graphGetCost(graph, node, node2) < CONFLICT_LIMIAR) break ;
				}

				if (otherPacket) continue ;

				if (delta > packet->ETA) {

					delta = packet->ETA;
				}

//#ifdef OLD
				neighbors = graphGetNeighbors(graph, node);
				i = 0;
				for (nodep = listBegin(neighbors); nodep; nodep = listNext(neighbors)) {

					if (arrayGet(backoff, * nodep)) i++;
				}

				if (time - packet->waitingSince <= i * 2 * GRAPH_MULTIPLIER) continue ;

				/*
				 * Lets priority block all neighbors.
				 */
				for (nodep = listBegin(neighbors); nodep; nodep = listNext(neighbors)) {

					arraySet(priorityBlockedNodes, * nodep, (void *) 1);
				}
//#endif				
				continue ;
			}

			/*
			 * Is the necessary link blocked?
			 */
			if (arrayGet(blockedLinks, simulationConflictNodeIndex(linkIndexBase, packet->flow, packet->currentHop))) {

				/*
				 * Place a block on the priorityBlock.
				 */
				neighbors = graphGetNeighbors(conflict, simulationConflictNodeIndex(linkIndexBase, packet->flow, packet->currentHop));
				for (link = listBegin(neighbors); link; link = listNext(neighbors)) {

					arrayInc(priorityBlockedLinks, * link);
				}

				stateAddTransmission(state, simulationConflictNodeIndex(linkIndexBase, packet->flow, packet->currentHop), packet->ETA, 1, packet->retries, time - packet->waitingSince);
				continue ;
			}

			/*
			 * is the link priority blocked?
			 */
			if (arrayGet(priorityBlockedLinks, simulationConflictNodeIndex(linkIndexBase, packet->flow, packet->currentHop))) {

				/*
				 * Place a block on the priorityBlock.
				 */
				neighbors = graphGetNeighbors(conflict, simulationConflictNodeIndex(linkIndexBase, packet->flow, packet->currentHop));
				for (link = listBegin(neighbors); link; link = listNext(neighbors)) {

					arrayInc(priorityBlockedLinks, * link);
				}

				stateAddTransmission(state, simulationConflictNodeIndex(linkIndexBase, packet->flow, packet->currentHop), packet->ETA, 1, packet->retries, time - packet->waitingSince);
				continue ;
			}

			/*
			 * is the source node priority blocked?
			 */
			if (arrayGet(priorityBlockedNodes, (long) arrayGet(arrayGet(paths, packet->flow), packet->currentHop))) {

				/*
				 * Place a block on the priorityBlock.
				 */
				neighbors = graphGetNeighbors(conflict, simulationConflictNodeIndex(linkIndexBase, packet->flow, packet->currentHop));
				for (link = listBegin(neighbors); link; link = listNext(neighbors)) {

					arrayInc(priorityBlockedLinks, * link);
				}

				stateAddTransmission(state, simulationConflictNodeIndex(linkIndexBase, packet->flow, packet->currentHop), packet->ETA, 1, packet->retries, time - packet->waitingSince);
				continue ;
			}

			/*
			 * If we got here, the link is not blocked in any way. Transmit the packet.
			 * See if there is a coding partner.
			 */
			path = arrayGet(paths, packet->flow);
			
			codedPacket = queuesFindCodingPartner(graph, queues, packet, paths,
					(unsigned long) arrayGet(path, packet->currentHop),
					blockedLinks, priorityBlockedLinks, linkIndexBase,
					& successProb1, & successProb2);
			
			/*
			 * Block links.
			 */

			neighbors = graphGetNeighbors(conflict, simulationConflictNodeIndex(linkIndexBase, packet->flow, packet->currentHop));
			for (link = listBegin(neighbors); link; link = listNext(neighbors)) {

				arrayInc(blockedLinks, * link);
			}

			/*
			 * Packet is going to be transmitted. Check if this is the first link
			 * of the first flow, in which case, we have to save this state.
			 */
			if (packet->currentHop == 0 && packet->flow == 0 && packet->retries == 0) haveToSaveState = 1;
			// printf("CURRENT HOP: %d FLOW: %d RETRIES: %d\n",packet->currentHop, packet->flow, packet->retries);

			/*
			 * Update hop and ETA.
			 */
//printf("Acessing pos %d with value %lu\n", simulationConflictNodeIndex(linkIndexBase, packet->flow, packet->currentHop), airTime[simulationConflictNodeIndex(linkIndexBase, packet->flow, packet->currentHop)]);
			packet->ETA = airTime[simulationConflictNodeIndex(linkIndexBase, packet->flow, packet->currentHop)];
			packet->currentHop = -(packet->currentHop + 1);
			// printf("FLOW: %d, NEXT PACKET IN: %d\n", packet->flow, (int) arrayGet(scheduleFlowTime, packet->flow));
			stateAddTransmission(state, simulationConflictNodeIndex(linkIndexBase, packet->flow, -packet->currentHop - 1), packet->ETA, 0, packet->retries, time - packet->waitingSince);
//printf("time %.2f: transmiting packet between %d and %d\n", time, (long) arrayGet(path, packet->currentHop), (long) arrayGet(path, packet->currentHop + 1));
			/*
			 * Move the packet to 
			 * the onTransmission list.
			 * Remove the node from the waitingNodes list.
			 */
			listAdd(onTransmissionPackets, packet);
			listDelCurrent(waitingNodes);

			/*
			 * Update delta, if necessary.
			 */
			if (delta > packet->ETA) {

				delta = packet->ETA;
			}

			/*
			 * If there is a coding partner, do the same with it.
			 */
			if (codedPacket) {

				tmp = sqrt((double) GRAPH_MULTIPLIER / (double) graphGetCost(graph, (long) arrayGet(path, -packet->currentHop - 1),
				                                        (long) arrayGet(path, -packet->currentHop)));
				packet->deliveryProbability *= tmp * successProb1;
				packet->ETA = GRAPH_MULTIPLIER;
				stateAddTransmission(state, simulationConflictNodeIndex(linkIndexBase, packet->flow, -packet->currentHop - 1), packet->ETA, 0, packet->retries, time - packet->waitingSince);
				packet->maxRetries = 1;

				packet = codedPacket;
				path = arrayGet(paths, packet->flow);
				queuesDelPacket(queues, packet, (unsigned long) arrayGet(path, packet->currentHop));

				/*
				 * Block links.
				 */

				neighbors = graphGetNeighbors(conflict, simulationConflictNodeIndex(linkIndexBase, packet->flow, packet->currentHop));
				for (link = listBegin(neighbors); link; link = listNext(neighbors)) {

					arrayInc(blockedLinks, * link);
				}

				/*
				 * Update hop and ETA.
				 */

//printf("Acessing pos %d with value %lu\n", simulationConflictNodeIndex(linkIndexBase, packet->flow, packet->currentHop), airTime[simulationConflictNodeIndex(linkIndexBase, packet->flow, packet->currentHop)]);
//				packet->ETA = airTime[simulationConflictNodeIndex(linkIndexBase, packet->flow, packet->currentHop)];
				tmp = sqrt((double) GRAPH_MULTIPLIER / (double) graphGetCost(graph, (long) arrayGet(path, packet->currentHop),
				                                        (long) arrayGet(path, packet->currentHop + 1)));
				packet->deliveryProbability *= tmp * successProb2;
				packet->ETA = GRAPH_MULTIPLIER;
				packet->waitingSince = 0;
				packet->currentHop = -(packet->currentHop + 1);
				packet->maxRetries = 1;
				/*
				 * We need to set retries because this packet did not go through
				 * backoff.
				 */
				packet->retries = 0;

				stateAddTransmission(state, simulationConflictNodeIndex(linkIndexBase, packet->flow, -packet->currentHop - 1), packet->ETA, 0, packet->retries, time - packet->waitingSince);
				
				/*
				 * Move the packet to 
				 * the onTransmission list.
				 */
				listAdd(onTransmissionPackets, packet);

				/*
				 * Update delta, if necessary.
				 */
				if (delta > packet->ETA) {

					delta = packet->ETA;
				}
			}
			else {

				if (packet->retries == 0) {

					tmp = 1 - sqrt((double) GRAPH_MULTIPLIER / (double) graphGetCost(graph, (long) arrayGet(path, -packet->currentHop - 1),
										(long) arrayGet(path, -packet->currentHop)));
					packet->deliveryProbability *= (1 - tmp * tmp * tmp * tmp);
				}
			}

			
			//if (packet->currentHop == -1) {
			//
			//	MALLOC(newPacket, sizeof(t_packet));
			//	newPacket->initialTime = time;
			//	newPacket->currentHop = 0;
			//	newPacket->flow = packet->flow;
			//	newPacket->deliveryProbability = 1.0;
			//	queuesAddPacket(queues, newPacket, (long) arrayGet(arrayGet(paths, newPacket->flow), 0));
			//}
			
		}


		if (haveToSaveState) {

			for (i = (long) listBegin(activeNodes); i; i = (long) listNext(activeNodes)) {

				node = i - 1;

				/* 
				 * If the queue of the node is not empty,
				 * we shall place all packets in the 
				 * current state.
				 */
				if (queuesSize(queues, node) > 0) {

					nodeQueue = queuesNodeQueue(queues, node);
					for (packet = listBegin(nodeQueue); packet; packet = listNext(nodeQueue)) 
						stateAddBuffer(state, simulationConflictNodeIndex(linkIndexBase, packet->flow, packet->currentHop));
				}
			}
		}

//if (targetTime != GRAPH_INFINITY) {
//printf("At " WEIGHT_FORMAT ":", time);
//statePrint(state);
//}
		if (targetTime == GRAPH_INFINITY) {
			
			if (haveToSaveState) {
				stateSetCurrentTime(state, time);
				stateSetDeliveredPackets(state, deliveredPackets);

				stateSetDeliveredPacketsFlows(state, permanentDeliveredPacketsFlows);
				stateSetSentPacketsFlows(state, permanentSentPacketsFlows);
				stateSetDelayFlows(state, delayFlows);
				//printf("State saved! Time: %d\n", state->currentTime);
//if (time > 10000){
//printf(WEIGHT_FORMAT "(%d) ", time, deliveredPackets);		
//fflush(stdout);
//break ;
//}
//printf("At " WEIGHT_FORMAT ":", time);
//statePrint(state);
				oldState = stateLookupAndStore(state, stateStorage);

				if (oldState != NULL) {
					for (i = 0; i < numberOfFlows; i++) {
					//	printf("Flow %d: %d pkts delivered\n",i,(unsigned long) arrayGet(deliveredPacketsFlows, i));
						if ((unsigned long) arrayGet(deliveredPacketsFlows, i) < 1) break ;
					}
					targetTime = time + stateGetCurrentTime(state) - stateGetCurrentTime(oldState);
					oldState = state;
//printf("setting targetTime to %lu\n", targetTime);
					continue ;
				}
#ifdef OLD				
				else {

					numberOfStates++;
#define MAX_NUMBER_OF_STATES	1000
					if (numberOfStates >= MAX_NUMBER_OF_STATES) {
					
						meanInterval = (float) time / (float) deliveredPackets;
						break ;
					}
				}
#endif				
			}
			else {
				// printf("State was not saved\n");
				stateFree(state);
				free(state);
			}
//#ifdef OLD
			/*
			 * Check if our time constraint has been reached.
			 */
			
			for (i = 0; i < numberOfFlows; i++) {
			//	printf(">>>> Flow %d: %d pkts delivered\n",i,(unsigned long) arrayGet(deliveredPacketsFlows, i));
				if ((unsigned long) arrayGet(deliveredPacketsFlows, i) < 1) break ;
			}

			if (i == numberOfFlows) {

				for (i = 0; i < numberOfFlows; i++) arraySet(deliveredPacketsFlows, i, 0);
//printf("Closing cicle %d at time %ld with %f delivered packets\n", lookAhead, time, deliveredPackets);
				if (lookAhead > 1) {

					meanTime = meanTime * (1 - alfa) + (time - oldTime) * alfa;
					//meanDelivery = meanDelivery * (1 - alfa) + (deliveredPackets - oldDeliveredPackets) * alfa;
//printf("Diff cicle: time %ld | delivered packets %f\n", time - oldTime, deliveredPackets - oldDeliveredPackets);
//printf("Mean cicle: time %f | delivered packets %f\n", meanTime, meanDelivery);

					//printf("Cicle - Mean time %.2f (Diff: %ld)\n", meanTime, time - oldTime);
					for (int j = 0; j < numberOfFlows; j++){
						float meanDPF = meanDeliveryPerFlow[j] * (1 - alfa) + (deliveredPacketsPerFlow[j] - oldDeliveredPacketsPerFlow[j]) * alfa;
						//printf("FLOW %d - Mean delivered: %f (Diff: %f)\n", j, meanDPF, (deliveredPacketsPerFlow[j] - oldDeliveredPacketsPerFlow[j]));
						//printf("Flow: %d - Mean Delivery: %ld - DPF: %ld - OLD DPF: %ld\n", j, (long) arrayGet(meanDeliveryPerFlow, j), (long) arrayGet(deliveredPacketsPerFlow, j), (long) arrayGet(oldDeliveredPacketsPerFlow, j));
						meanDeliveryPerFlow[j] = meanDPF;
						meanSentPerFlow[j] = meanSentPerFlow[j] * (1 - alfa) + ((long) arrayGet(permanentSentPacketsFlows,j) - oldSentPacketsPerFlow[j]) * alfa;
						meanDelayFlows[j] = meanDelayFlows[j] + ((long) arrayGet(delayFlows,j) - oldDelayFlows[j]);
					}
				}
				else {

					meanTime = time;
					//meanDelivery = deliveredPackets;
					for (int j = 0; j < numberOfFlows; j++) {
						meanDeliveryPerFlow[j] = deliveredPacketsPerFlow[j];
						meanSentPerFlow[j] = (long) arrayGet(permanentSentPacketsFlows,j);
						meanDelayFlows[j] = (long) arrayGet(delayFlows,j);
					}
				}
				oldTime = time;
				//oldDeliveredPackets = deliveredPackets;
				//meanInterval = meanTime / meanDelivery; 
				for (int j = 0; j < numberOfFlows; j++) {
					oldDeliveredPacketsPerFlow[j] = deliveredPacketsPerFlow[j];
					oldSentPacketsPerFlow[j] = (long) arrayGet(permanentSentPacketsFlows,j);	
					oldDelayFlows[j] = (long) arrayGet(delayFlows,j);
					r->meanIntervalPerFlow[j] = meanTime / meanDeliveryPerFlow[j];
					//printf("FLOW: %d MEANDPF: %ld MEANIPF: %ld\n", j, arrayGet(meanDeliveryPerFlow, j), arrayGet(meanIntervalPerFlow,j));
				}

				lookAhead++;

				if (lookAhead > 30000) {

					int intervalsSmallDiff = 1;
					for (int j = 0; j < numberOfFlows; j++){
						float meanIntervalRatio = oldMeanIntervalPerFlow[j] / r->meanIntervalPerFlow[j];
						if(meanIntervalRatio < 0.99 && meanIntervalRatio > 1.01){
							intervalsSmallDiff = 0;
						}
					} 

					if (lookAhead > maxDeliveredPerFlow || intervalsSmallDiff) {
//printf("Search for cycle timed-out\n");
							r->cost=0;
							r->delay=0;
							for (int f = 0; f < numberOfFlows; f++) {
								//r->meanIntervalPerFlow[f] /= correctionFactors[f];
								// r->packetsLossPerFlow[f] = 100 * (1 - (correctionFactors[f] * meanDeliveryPerFlow[f]) / meanSentPerFlow[f]);
								r->packetsLossPerFlow[f] = 100 * (1 - (meanDeliveryPerFlow[f] / meanSentPerFlow[f]));	
								r->delayFlows[f] = meanDelayFlows[f] / meanDeliveryPerFlow[f];	

								float flowTime=(int) arrayGet(flowTimes, f);
								float meanInterval = r->meanIntervalPerFlow[f];
								r->cost= r->cost + ((meanInterval-flowTime)/meanInterval);
								r->rateFlows[f]= 8000/(r->meanIntervalPerFlow[f]*0.000054978);
								r->delayFlows[f]= r->delayFlows[f]/1000;
								float delay = r->delayFlows[f];
								r->delay =  r->delay+(delay/300);
								r->optimal->packetSentBetweenStates[f] = -1;
							}

							//printf("Flow: %d - Mean Delivery: %f - Mean Interval: %f\n", j, meanDeliveryPerFlow[j], r->meanIntervalPerFlow[j]);
							//printf("Flow: %d - Mean Delivery: %ld - Mean Interval: %ld\n", j, (long) arrayGet(meanDeliveryPerFlow, j), (long) arrayGet(meanIntervalPerFlow, j));
					
						break ;
					}

					/*
					if (lookAhead > maxDeliveredPerFlow || (oldMeanInterval / meanInterval > 0.99 && oldMeanInterval / meanInterval < 1.01)) {
printf("Search for cycle timed-out\n");
printf("meanInterval = %f\n", meanInterval);
						break ;
					}
					*/
				}

				//oldMeanInterval = meanInterval;
				for (int j = 0; j < numberOfFlows; j++) oldMeanIntervalPerFlow[j] = r->meanIntervalPerFlow[j];

			}
//#endif
		} else {

			if (targetTime <= time) {
				//printf("Found perfect solution. Cicle between %lu and %lu\n", stateGetCurrentTime(oldState), time);
				if (deliveredPackets) {
					r->cost=0;
					r->delay=0;
					for (int f = 0; f < numberOfFlows; f++){ 
						double deliveredPacketsFlowsBetweenStates = (((long) arrayGet(permanentDeliveredPacketsFlows,f)) / (float) GRAPH_MULTIPLIER) - (((long) arrayGet(stateGetDeliveredPacketsFlows(oldState),f)) / (float) GRAPH_MULTIPLIER);
						long sentPacketsFlowsBetweenStates = (long) (arrayGet(permanentSentPacketsFlows,f) - arrayGet(stateGetSentPacketsFlows(oldState),f));

						//printf("lossPacketsFlowsBetweenStates = %d ,  deliveredPacketsFlowsBetweenStates = %f\n", lossPacketsFlowsBetweenStates, deliveredPacketsFlowsBetweenStates);
						//printf("Flow %d, deliveredPacketsFlowsBetweenStates = %.0f\n", f, deliveredPacketsFlowsBetweenStates);
						//printf("Flow %d:\n", f);
						if (deliveredPacketsFlowsBetweenStates != 0){

							float timeBetweenStates = ((float) (time - stateGetCurrentTime(oldState)));

							r->meanIntervalPerFlow[f] = timeBetweenStates / deliveredPacketsFlowsBetweenStates;
							r->packetsLossPerFlow[f] = 100 * (1 - deliveredPacketsFlowsBetweenStates / sentPacketsFlowsBetweenStates);
							//r->delayFlows[f] = ((long) delayFlows - ((long) arrayGet(stateGetDelayFlows(oldState),f))) / deliveredPacketsFlowsBetweenStates;
							//r->delayFlows[f]=(long) (arrayGet(stateGetDelayFlows(state),f) - arrayGet(stateGetDelayFlows(oldState),f))/deliveredPacketsFlowsBetweenStates;
							r->delayFlows[f]=(long) (arrayGet(delayFlows,f) - arrayGet(stateGetDelayFlows(oldState),f))/deliveredPacketsFlowsBetweenStates;
							


							//long d = ((long) delayFlows - ((long) arrayGet(stateGetDelayFlows(oldState),f))) / deliveredPacketsFlowsBetweenStates;

							//printf("Current STATE Time: %f\n", (float) stateGetCurrentTime(state));
							//printf("Last saved STATE Time: %f\n", (float) stateGetCurrentTime(oldState));

							r->optimal->packetSentBetweenStates[f] = sentPacketsFlowsBetweenStates;
							r->optimal->packetDeliveredBetweenStates[f] = deliveredPacketsFlowsBetweenStates;
							r->optimal->timeBetweenStates[f] = timeBetweenStates;

							//printf(">>>> MeanIntervalPerFlow = %f\n", r->meanIntervalPerFlow[f]);

							//printf(">>>> SentPacketsFlowsBetweenStates = %ld (%ld-%ld)\n", sentPacketsFlowsBetweenStates, (long) arrayGet(permanentSentPacketsFlows,f), (long) arrayGet(stateGetSentPacketsFlows(oldState),f));
							//printf(">>>> DeliveredPacketsFlowsBetweenStates = %.4f (%.4f-%.4f)\n", deliveredPacketsFlowsBetweenStates, (((long) arrayGet(permanentDeliveredPacketsFlows,f)) / (float) GRAPH_MULTIPLIER) , (((long) arrayGet(stateGetDeliveredPacketsFlows(oldState),f)) / (float) GRAPH_MULTIPLIER));
							//printf(">>>> TimeBetweenStates = %.0f (%.0f - %.0f)\n", timeBetweenStates, (float) time, (float) stateGetCurrentTime(oldState));
						} else {
						    r->meanIntervalPerFlow[f] = GRAPH_INFINITY;
							r->packetsLossPerFlow[f] = 100; // None delivered, All packets lost.
							r->delayFlows[f] = 0;
							
						}
						float flowTime=(int) arrayGet(flowTimes, f);
						float meanInterval = r->meanIntervalPerFlow[f];
						if (meanInterval<flowTime) meanInterval=flowTime;
						r->cost= r->cost + ((meanInterval-flowTime)/meanInterval);
						r->rateFlows[f]= 8000/(meanInterval*0.000054978);
						r->delayFlows[f]= r->delayFlows[f]/1000;
						float delay = r->delayFlows[f];
						r->delay =  r->delay+(delay/300);
						
						//r->rateFlows[f]=r->meanIntervalPerFlow[f];
//printf("meanIntervalFlow = %f\n", r->meanIntervalFlow[f]);
//printf("meanIntervalFlowPermanent = %f\n",(float) (time/correctionFactors[f]/(long)arrayGet(permanentDeliveredPacketsFlows, f)));
			
					}

					/*
					printf("DeliveredPackets: new = %f old = %f\n", deliveredPackets, stateGetDeliveredPackets(oldState));
					double deliveredPacketsBetweenStates = ((float) (deliveredPackets - stateGetDeliveredPackets(oldState)));
					if (deliveredPacketsBetweenStates != 0)
						meanInterval = ((float) (time - stateGetCurrentTime(oldState))) / deliveredPacketsBetweenStates;
					else {

							meanInterval = GRAPH_INFINITY;
					}
					*/
//printf("DeliveredPackets = %f, deliveredPacketsBetweenStates = %f\n", deliveredPackets, deliveredPacketsBetweenStates);
				}
				else{ 
					r->cost =GRAPH_INFINITY;
					r->delay =GRAPH_INFINITY;
					meanInterval = GRAPH_INFINITY;
				}
				stateFree(state);
				free(state);
				stateFree(oldState);
				free(oldState);

//printf("DeliveredPackets = %f\n", deliveredPackets);
//printf("meanInterval = %f\n", meanInterval);

				break ;
			}
			else {
				stateFree(state);
				free(state);
			}
		}

	}

	for (i = 0; i < graphSize(graph); i++) {

		if ((packet = arrayGet(backoff, i)) == NULL) continue ;

		free(packet);
	}





	free(deliveredPacketsPerFlow);
	free(oldDeliveredPacketsPerFlow);
	free(meanDeliveryPerFlow);
	free(oldMeanIntervalPerFlow);
	free(meanSentPerFlow);
	free(oldSentPacketsPerFlow);
	free(meanDelayFlows);
	free(oldDelayFlows);

	arrayFree(permanentDeliveredPacketsFlows);
	free(permanentDeliveredPacketsFlows);
	arrayFree(permanentSentPacketsFlows);
	free(permanentSentPacketsFlows);

	arrayFree(delayFlows);
	free(delayFlows);
	arrayFree(idPacketFlows);
	free(idPacketFlows);


	free(backoffUnit);
	free(numberOfRetries);
	arrayFree(backoff);
	free(backoff);
	arrayFree(deliveredPacketsFlows);
	free(deliveredPacketsFlows);
	arrayFree(blockedLinks);
	free(blockedLinks);
	arrayFree(priorityBlockedNodes);
	free(priorityBlockedNodes);
	arrayFree(priorityBlockedLinks);
	free(priorityBlockedLinks);
	free(linkIndexBase);
	stateStorageFreeWithData(stateStorage);
	free(stateStorage);
	graphFree(conflict);
	free(conflict);
	queuesFree(queues);
	free(queues);
	listFree(waitingNodes);
	free(waitingNodes);
	listFree(onTransmissionPackets);
	free(onTransmissionPackets);
	free(airTime);
//printf("Leaving at %lu and returning %.2f\n", times(NULL), meanInterval);
//printf("We had %u packets at %llu and %u packets at %llu\n", stateGetDeliveredPackets(oldState), stateGetCurrentTime(oldState), stateGetDeliveredPackets(state), stateGetCurrentTime(state));

	/*
	 * Return the average interval.
	 */
	
	return(r);
}
		


