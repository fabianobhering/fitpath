#define _ISOC99_SOURCE
#include <math.h>
#include <string.h>

//#include <sys/times.h>

#include "simulation.h"
#include "memory.h"
#include "list.h"
#include "graph.h"
#include "array.h"
#include "stack.h"
#include "state.h"

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

	int flow;
	t_weight initialTime;
	int currentHop;
	t_weight ETA;
	int lastIteration;
} t_packet;

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

	if ((unsigned long) arrayGet(queues->localQueue[node].individualFlowCounts, packet->flow) == queues->queueLimit) {

		for (last = listEnd(queues->localQueue[node].packets); last; last = listPrev(queues->localQueue[node].packets)) {

			if (last->flow == packet->flow) {

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
									int * linkIndexBase) {
//return(NULL);
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
		if (prevHopP2 != nextHopP1 && graphGetCost(graph, prevHopP2, nextHopP1) > (double) GRAPH_MULTIPLIER / 0.8) continue ;
//printf("Previous hop match 1\n");

		nextHopP2 = (unsigned long) arrayGet(arrayGet(paths, p->flow), p->currentHop + 1);
		if (prevHopP1 != nextHopP2 && graphGetCost(graph, prevHopP1, nextHopP2) > (double) GRAPH_MULTIPLIER / 0.8) continue ;
//printf("Previous hop match 2\n");

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

					if (graphGetCost(graph, head1, head2) < GRAPH_INFINITY) {

						graphAddLink(conflict, 
							simulationConflictNodeIndex(linkIndexBase, i, j),
							simulationConflictNodeIndex(linkIndexBase, k, l),
							1);
						graphAddLink(conflict, 
							simulationConflictNodeIndex(linkIndexBase, k, l),
							simulationConflictNodeIndex(linkIndexBase, i, j),
							1);
					}
					else if (graphGetCost(graph, head2, head1) < GRAPH_INFINITY) {

						graphAddLink(conflict, 
							simulationConflictNodeIndex(linkIndexBase, i, j),
							simulationConflictNodeIndex(linkIndexBase, k, l),
							1);
						graphAddLink(conflict, 
							simulationConflictNodeIndex(linkIndexBase, k, l),
							simulationConflictNodeIndex(linkIndexBase, i, j),
							1);
					}
//					else if (graphGetCost(graph, tail1, head2) < GRAPH_INFINITY) {
					else if (graphGetCost(graph, head2, tail1) < GRAPH_INFINITY) {

						graphAddLink(conflict, 
							simulationConflictNodeIndex(linkIndexBase, i, j),
							simulationConflictNodeIndex(linkIndexBase, k, l),
							1);
						graphAddLink(conflict, 
							simulationConflictNodeIndex(linkIndexBase, k, l),
							simulationConflictNodeIndex(linkIndexBase, i, j),
							1);
					}
					else if (graphGetCost(graph, head1, tail2) < GRAPH_INFINITY) {

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

#define STATE_HASH_SIZE		1024

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

float simulationSimulate(t_graph * graph, t_array * paths) {

	int * linkIndexBase;
	t_graph * conflict;
	t_array * path;
	t_list * neighbors, * nodeQueue;
	t_list * waitingNodes, * onTransmissionPackets;
	t_array * blockedLinks, * priorityBlockedLinks;
	t_packet * newPacket, * packet, * codedPacket;
	t_state * oldState, * state;
	t_stateStorage * stateStorage;
	t_array * deliveredPacketsFlows, * permanentDeliveredPacketsFlows;
	t_weight time, oldTime, delta, oldDelta;
	int i, j, k;
	int * link;
	long deliveredPackets, oldDeliveredPackets;
	int slots;
	int numberOfFlows;
	long node, lastNode;
	float meanInterval, oldMeanInterval, meanTime, meanDelivery;
	t_queues * queues;
	int lookAhead = 1;
	int maxDeliveredPerFlow = 100;
	float alfa = 0.8;
	float * correctionFactors;
	float correctionFactor;
	double tmp;
	int maxFlowsPerNode = 0;
	t_array * flowsPerNode;
	int haveToSaveState;
	int numberOfStates = 0;
	t_weight * airTime;
	int numberOfLinks = 0;

	numberOfFlows = arrayLength(paths);

	deliveredPacketsFlows = arrayNew(numberOfFlows);
	for (i = 0; i < numberOfFlows; i++) arraySet(deliveredPacketsFlows, i, 0);

	permanentDeliveredPacketsFlows = arrayNew(numberOfFlows);
	for (i = 0; i < numberOfFlows; i++) arraySet(permanentDeliveredPacketsFlows, i, 0);

	flowsPerNode = arrayNew(graphSize(graph));
	arrayClear(flowsPerNode);
	MALLOC(correctionFactors, sizeof(float) * numberOfFlows);
	for (i = 0; i < numberOfFlows; i++) {
//printf("%ld ", (long) arrayGet(arrayGet(paths, i), 0));
		correctionFactors[i] = 1;

		node = (long) arrayGet(arrayGet(paths, i), 0);
		arrayInc(flowsPerNode, node);
		if ((long) arrayGet(flowsPerNode, node) > maxFlowsPerNode) maxFlowsPerNode = (long) arrayGet(flowsPerNode, node);

		numberOfLinks += (arrayLength(arrayGet(paths, i)) - 1);
		for (j = 1; j < arrayLength(arrayGet(paths, i)); j++) {
//printf("%ld ", (long) arrayGet(arrayGet(paths, i), j));
			lastNode = (long) arrayGet(arrayGet(paths, i), j - 1);
			node = (long) arrayGet(arrayGet(paths, i), j);
			tmp = (double) GRAPH_MULTIPLIER / (double) graphGetCost(graph, lastNode, node);
			tmp = 1 - sqrt(tmp);
			if (tmp > 0)
				correctionFactors[i] *= (1 - exp(4 * log(tmp)));

			arrayInc(flowsPerNode, node);
			if ((long) arrayGet(flowsPerNode, node) > maxFlowsPerNode) maxFlowsPerNode = (long) arrayGet(flowsPerNode, node);
		}
//printf("\n");
	}

	arrayFree(flowsPerNode);
	free(flowsPerNode);

	MALLOC(airTime, sizeof(t_weight) * numberOfLinks);
	k = 0;
	for (i = 0; i < numberOfFlows; i++) {

		for (j = 1; j < arrayLength(arrayGet(paths, i)); j++) {

			lastNode = (long) arrayGet(arrayGet(paths, i), j - 1);
			node = (long) arrayGet(arrayGet(paths, i), j);
			tmp = (double) GRAPH_MULTIPLIER / (double) graphGetCost(graph, lastNode, node);
			if (tmp > 0) {

				//airTime[k++] = GRAPH_MULTIPLIER * tmp * (1 + (1-tmp) * (2 + (1-tmp) * (3 + 4 * (1-tmp)))) / (1 - exp(4 * log(1 - tmp)));
				airTime[k++] = GRAPH_MULTIPLIER * ((tmp * (1 + (1-tmp) * (2 + (1-tmp) * 3))) + 4 * (1-tmp) * (1-tmp) * (1-tmp) * (1-tmp));				
			}
			else {

				airTime[k++] = 4 * GRAPH_MULTIPLIER;
			}
//printf("Airtime = %lu, tmp = %f\n", airTime[k-1], tmp);
		}
	}

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

	/*
	 * We'll keep an updated state of the links which 
	 * are currently blocked.
	 */
	blockedLinks = arrayNew(graphSize(conflict));
	priorityBlockedLinks = arrayNew(graphSize(conflict));
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
	state = stateNew(slots);

	/*
	 * Initialization: iterate through all flows and put
	 * their first packets either on flow or on buffer.
	 */
	for (i = 0; i < numberOfFlows; i++) {

		path = arrayGet(paths, i);

		MALLOC(newPacket, sizeof(t_packet));
		newPacket->initialTime = time;
		newPacket->flow = i;

		/*
		 * Is the necessary link blocked?
		 */
		if (arrayGet(blockedLinks, simulationConflictNodeIndex(linkIndexBase, i, 0))) {

			/*
			 * Yes, packet stays in hop 0.
			 */
			newPacket->currentHop = 0;

			/*
			 * Add the information about this packet staying on the
			 * buffer to the current state.
			 */
			stateAddBuffer(state, simulationConflictNodeIndex(linkIndexBase, i, newPacket->currentHop));

			/*
			 * Add the packet to the node's queue.
			 */
			queuesAddPacket(queues, newPacket, (long) arrayGet(path, 0));

			/*
			 * Also add the correspondent node to the 
			 * waitingNodes list,
			 * so that we know this node is disputing
			 * the wireless medium. We only need to do
			 * that if this is the first packet in the
			 * node's buffer (otherwise, the node is 
			 * already in the waitingNodes list).
			 */
			if (queuesSize(queues, (long) arrayGet(path, 0)) == 1)
				listAdd(waitingNodes, (void *) (((long) arrayGet(path, 0)) + 1));
		}
		else {

			/*
			 * No, it is transmitted. We coded that
			 * by saying the packet is in the -1 hop.
			 */
			newPacket->currentHop = -1;
//printf("Acessing pos %d with value %lu\n", simulationConflictNodeIndex(linkIndexBase, i, 0), airTime[simulationConflictNodeIndex(linkIndexBase, i, 0)]);
			newPacket->ETA = airTime[simulationConflictNodeIndex(linkIndexBase, i, 0)];

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
			stateAddTransmission(state, simulationConflictNodeIndex(linkIndexBase, i, 0), newPacket->ETA);

			/*
			 * Also add it to the onTransmissionPackets list,
			 * so that we know this packet is disputing
			 * the wireless medium.
			 */
			listAdd(onTransmissionPackets, newPacket);

			MALLOC(newPacket, sizeof(t_packet));
			newPacket->initialTime = time;
			newPacket->currentHop = 0;
			newPacket->flow = i;
			queuesAddPacket(queues, newPacket, (long) arrayGet(arrayGet(paths, newPacket->flow), 0));
			if (queuesSize(queues, (long) arrayGet(arrayGet(paths, newPacket->flow), 0)) == 1)
				listAdd(waitingNodes, (void *) (((long) arrayGet(arrayGet(paths, newPacket->flow), 0)) + 1));
			stateAddBuffer(state, simulationConflictNodeIndex(linkIndexBase, newPacket->flow, newPacket->currentHop));
		}
	}

	stateSetCurrentTime(state, 0.0);
	stateSetDeliveredPackets(state, 0);
//printf("At " WEIGHT_FORMAT ":", time);
//statePrint(state);
	stateLookupAndStore(state, stateStorage);
	numberOfStates++;

	/*
	 * Main loop:
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
		state = stateNew(slots);
		arrayClear(priorityBlockedLinks);

		/*
		 * Update time variables.
		 */
		time += delta;
		oldDelta = delta;
		delta = GRAPH_INFINITY;

		for (packet = listBegin(onTransmissionPackets); packet; packet = listNext(onTransmissionPackets)) {

			packet->ETA -= oldDelta;

			if (packet->ETA <= 0) {

				/*
				 * This packet has just arrived at its
				 * next hop. Fix the fields.
				 */
				packet->currentHop *= -1;

//printf("At time %d, Packet just arrived at hop %d for flow %d\n", time, packet->currentHop, packet->flow);
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
#ifdef OLD
				/*
				 * Did the packet just used the first hop?
				 */
				if (packet->currentHop == 1) {

					/*
					 * Yes, it means we have to place a new 
					 * packet competing for the first link.
					 */

					MALLOC(newPacket, sizeof(t_packet));
					newPacket->initialTime = time;
					newPacket->currentHop = 0;
					newPacket->flow = packet->flow;
					queuesAddPacket(queues, newPacket, (long) arrayGet(arrayGet(paths, newPacket->flow), 0));
					if (queuesSize(queues, (long) arrayGet(arrayGet(paths, newPacket->flow), 0)) == 1)
						listAdd(waitingNodes, (void *) (((long) arrayGet(arrayGet(paths, newPacket->flow), 0)) + 1));
				}
#endif
				/*
				 * Has the packet arrived in its final
				 * destination?
				 */
				if (packet->currentHop == arrayLength(arrayGet(paths, packet->flow)) - 1) {

					/*
					 * Yes. Update the number of delivered packets and
					 * free the packet.
					 */
//printf("Packet just delivered for flow %d\n", packet->flow);
					arrayInc(permanentDeliveredPacketsFlows, packet->flow);
					arrayInc(deliveredPacketsFlows, packet->flow);
					free(packet);
					deliveredPackets++;
				}
				else {

					/*
					 * Add the packet to the node's queue and the node to
					 * the waitingNodes list (if it is not already there).
					 */
					queuesAddPacket(queues, packet, (long) arrayGet(arrayGet(paths, packet->flow), packet->currentHop));
					if (queuesSize(queues, (long) arrayGet(arrayGet(paths, packet->flow), packet->currentHop)) == 1)
						listAdd(waitingNodes, (void *) (((long) arrayGet(arrayGet(paths, packet->flow), packet->currentHop)) + 1));
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

				stateAddTransmission(state, simulationConflictNodeIndex(linkIndexBase, packet->flow, -packet->currentHop - 1), packet->ETA);
			}
		}

		/*
		 * Now we loop through the list of the waiting nodes
		 * to see if we can transmit their packets.
		 */
		lastNode = (long) listEnd(waitingNodes) - 1;

		for (node = (long) listBegin(waitingNodes); node; node = (long) listNext(waitingNodes)) {

			/*
			 * Node is stored as node + 1 to avoid NULL values.
			 */
			node--;
//printf("Node %d\n", node);
			/* 
			 * Always take the first packet of the node's buffer.
			 */
			packet = queuesFirst(queues, node);

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

				/*
				 * If the node has packets waiting on its buffers,
				 * we must add that information to the state.
				 */

				nodeQueue = queuesNodeQueue(queues, node);
				for (packet = listBegin(nodeQueue); packet; packet = listNext(nodeQueue)) 
					stateAddBuffer(state, simulationConflictNodeIndex(linkIndexBase, packet->flow, packet->currentHop));
//printf("Here at line %d: ", __LINE__);
//statePrint(state);	
				if (node == lastNode) break ;

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

				/*
				 * If the node has packets waiting on its buffers,
				 * we must add that information to the state.
				 */

				nodeQueue = queuesNodeQueue(queues, node);
				for (packet = listBegin(nodeQueue); packet; packet = listNext(nodeQueue)) 
					stateAddBuffer(state, simulationConflictNodeIndex(linkIndexBase, packet->flow, packet->currentHop));
//printf("Here at line %d: ", __LINE__);
//statePrint(state);	
				if (node == lastNode) break ;

				continue ;
			}

			/*
			 * If we got here, the link is not blocked in any way. Transmit the packet.
			 * See if there is a coding partner.
			 */
			path = arrayGet(paths, packet->flow);
			codedPacket = queuesFindCodingPartner(graph, queues, packet, paths,
					(unsigned long) arrayGet(path, packet->currentHop),
					blockedLinks, priorityBlockedLinks, linkIndexBase);
//codedPacket = NULL;
			/*
			 * Take packet out of the node's buffer.
			 */
			queuesDelPacket(queues, packet, (unsigned long) arrayGet(path, packet->currentHop));

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
			if (packet->currentHop == 0 && packet->flow == 0) haveToSaveState = 1;

			/*
			 * Update hop and ETA.
			 */
//printf("Acessing pos %d with value %lu\n", simulationConflictNodeIndex(linkIndexBase, packet->flow, packet->currentHop), airTime[simulationConflictNodeIndex(linkIndexBase, packet->flow, packet->currentHop)]);
			packet->ETA = airTime[simulationConflictNodeIndex(linkIndexBase, packet->flow, packet->currentHop)];
			packet->currentHop = -(packet->currentHop + 1);

			stateAddTransmission(state, simulationConflictNodeIndex(linkIndexBase, packet->flow, -packet->currentHop - 1), packet->ETA);
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
				packet->ETA = airTime[simulationConflictNodeIndex(linkIndexBase, packet->flow, packet->currentHop)];
				packet->currentHop = -(packet->currentHop + 1);

				stateAddTransmission(state, simulationConflictNodeIndex(linkIndexBase, packet->flow, -packet->currentHop - 1), packet->ETA);
				
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

			if (packet->currentHop == -1) {

				MALLOC(newPacket, sizeof(t_packet));
				newPacket->initialTime = time;
				newPacket->currentHop = 0;
				newPacket->flow = packet->flow;
				queuesAddPacket(queues, newPacket, (long) arrayGet(arrayGet(paths, newPacket->flow), 0));
			}

			/* 
			 * If the queue of the node is not empty,
			 * the node still wants to transmit and
			 * now it should wait at the end of the 
			 * list.
			 */
			if (queuesSize(queues, node) > 0) {

				listAdd(waitingNodes, (void *) (node + 1));

				/*
				 * If the node has packets waiting on its buffers,
				 * we must add that information to the state.
				 */

				nodeQueue = queuesNodeQueue(queues, node);
				for (packet = listBegin(nodeQueue); packet; packet = listNext(nodeQueue)) 
					stateAddBuffer(state, simulationConflictNodeIndex(linkIndexBase, packet->flow, packet->currentHop));
//printf("Here at line %d: ", __LINE__);
//statePrint(state);
			}

			if (node == lastNode) break ;
		}
//haveToSaveState = 1;
		if (haveToSaveState) {

			stateSetCurrentTime(state, time);
			stateSetDeliveredPackets(state, deliveredPackets);
//if (time > 10000){
//printf(WEIGHT_FORMAT "(%d) ", time, deliveredPackets);		
//fflush(stdout);
//break ;
//}
//printf("At " WEIGHT_FORMAT ":", time);
//statePrint(state);
			oldState = stateLookupAndStore(state, stateStorage);

			if (oldState != NULL) {
printf("Found perfect solution. Cicle between %d and %d\n", stateGetCurrentTime(oldState), stateGetCurrentTime(state));
				correctionFactor = 0;
				for (i = 0; i < numberOfFlows; i++) {
printf("correctionFactors[%d] = %f\n", i, correctionFactors[i]);
					correctionFactor += correctionFactors[i] * (long) arrayGet(permanentDeliveredPacketsFlows, i);
				}
				if (deliveredPackets) {

					correctionFactor /= deliveredPackets;
printf("CorrectionFactor = %f\n", correctionFactor);
					double deliveredPacketsBetweenStates = (correctionFactor * ((float) (stateGetDeliveredPackets(state) - stateGetDeliveredPackets(oldState))));
					if (deliveredPacketsBetweenStates != 0)
						meanInterval = ((float) (stateGetCurrentTime(state) - stateGetCurrentTime(oldState))) / deliveredPacketsBetweenStates;
					else {

							meanInterval = GRAPH_INFINITY;
					}
printf("DeliveredPackets = %d, deliveredPacketsBetweenStates = %d\n", deliveredPackets, deliveredPacketsBetweenStates);
				}
				else 
					meanInterval = GRAPH_INFINITY;
				stateFree(state);
				free(state);
printf("DeliveredPackets = %d\n", deliveredPackets);
printf("meanInterval = %f -> %f\n", meanInterval * correctionFactor, meanInterval);
				break ;
			}
			else {

				numberOfStates++;
#define MAX_NUMBER_OF_STATES	1000
				if (numberOfStates >= MAX_NUMBER_OF_STATES) {
				
					meanInterval = (float) time / (float) deliveredPackets;
					break ;

				}
			}
		}
		else {

			stateFree(state);
			free(state);
		}

		/*
		 * Check if our time constraint has been reached.
		 */
		
		for (i = 0; i < numberOfFlows; i++) {

			if ((unsigned long) arrayGet(deliveredPacketsFlows, i) < 1) break ;
		}

		if (i == numberOfFlows) {

			for (i = 0; i < numberOfFlows; i++) arraySet(deliveredPacketsFlows, i, 0);
printf("Closing cicle %d at time %d with %d delivered packets\n", lookAhead, time, deliveredPackets);
			if (lookAhead > 1) {

				meanTime = meanTime * (1 - alfa) + (time - oldTime) * alfa;
				meanDelivery = meanDelivery * (1 - alfa) + (deliveredPackets - oldDeliveredPackets) * alfa;
printf("Diff cicle: time %ld | delivered packets %ld\n", time - oldTime, deliveredPackets - oldDeliveredPackets);
printf("Mean cicle: time %f | delivered packets %f\n", meanTime, meanDelivery);
			}
			else {

				meanTime = time;
				meanDelivery = deliveredPackets;
			}
			oldTime = time;
			oldDeliveredPackets = deliveredPackets;
			meanInterval = meanTime / meanDelivery; 

			lookAhead++;

			if (lookAhead > 3) {

				if (lookAhead > maxDeliveredPerFlow || (oldMeanInterval / meanInterval > 0.99 && oldMeanInterval / meanInterval < 1.01)) {
printf("Search for cycle timed-out\n");
					correctionFactor = 0;
					for (i = 0; i < numberOfFlows; i++) {
printf("correctionFactors[%d] = %f\n", i, correctionFactors[i]);
						correctionFactor += correctionFactors[i] * (long) arrayGet(permanentDeliveredPacketsFlows, i);
					}
					correctionFactor /= deliveredPackets;
printf("CorrectionFactor = %f\n", correctionFactor);
printf("meanInterval = %f -> %f\n", meanInterval, (float) meanInterval / correctionFactor);
					meanInterval /= correctionFactor;
					break ;
				}

			}

			oldMeanInterval = meanInterval;

		}

	}

	arrayFree(permanentDeliveredPacketsFlows);
	free(permanentDeliveredPacketsFlows);
	free(correctionFactors);
	arrayFree(deliveredPacketsFlows);
	free(deliveredPacketsFlows);
	arrayFree(blockedLinks);
	free(blockedLinks);
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
	listFreeWithData(onTransmissionPackets);
	free(onTransmissionPackets);
	free(airTime);
//printf("Leaving at %lu and returning %.2f\n", times(NULL), meanInterval);
//printf("We had %u packets at %llu and %u packets at %llu\n", stateGetDeliveredPackets(oldState), stateGetCurrentTime(oldState), stateGetDeliveredPackets(state), stateGetCurrentTime(state));

	/*
	 * Return the average interval.
	 */
	return(meanInterval);
}
		


