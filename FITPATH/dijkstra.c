#include "graph.h"
#include "array.h"
#include "set.h"
#include "list.h"
#include "memory.h"

#define _ISOC99_SOURCE
#include <math.h>
#include <stdio.h>

t_weight dijkstra(t_graph * graph, long source, long destination, t_array ** output) {

	t_array * lastHop, * numberOfHops;
	t_list * neighbors;
	t_set * defSet;
	t_list * defList;
	long bestSrcNode, bestDstNode;
	t_weight bestCost;
	int srcNode, * dstNode;
	long node;
	t_weight cost, * pathsCost;
	int i;
	int numberOfNodes, hopsForNode;

	numberOfNodes = graphSize(graph);

	lastHop = arrayNew(numberOfNodes);
	numberOfHops = arrayNew(numberOfNodes);
	defSet = setNew();
	defList = listNew();
	MALLOC(pathsCost, sizeof(t_weight) * numberOfNodes);

	setAdd(defSet, source);
	listAdd(defList, (void *) source + 1);
	arraySet(lastHop, source, (void *) source);
	pathsCost[source] = 0.0;
	arraySet(numberOfHops, source, 0);

	for (i = 1; i < numberOfNodes; i++) {

		bestSrcNode = -1;
		bestDstNode = -1;
		bestCost = GRAPH_INFINITY;

		for (srcNode = (unsigned long) listBegin(defList); srcNode; srcNode = (unsigned long) listNext(defList)) {

			srcNode -= 1;
			neighbors = graphGetNeighbors(graph, srcNode);
			for (dstNode = listBegin(neighbors); dstNode; dstNode = listNext(neighbors)) {

				if (graphIsDisabled(graph, srcNode, * dstNode)) continue ;
				if (setIsElementOf(defSet, * dstNode)) continue ;

				cost = pathsCost[srcNode] + graphGetCost(graph, srcNode, * dstNode);
				if (cost < bestCost) {

					bestCost = cost;
					bestSrcNode = srcNode;
					bestDstNode = * dstNode;
				}
			}
		}

		if (bestSrcNode == -1) {

//			fprintf(stderr, "Dijkstra could not find routes.\n");
			listFree(defList);
			setFree(defSet);
			arrayFree(lastHop);
			arrayFree(numberOfHops);
			free(pathsCost);
			free(defList);
			free(defSet);
			free(lastHop);
			free(numberOfHops);

			return(GRAPH_INFINITY);
		}

		setAdd(defSet, bestDstNode);
		listAdd(defList, (void *) bestDstNode + 1);
		arraySet(lastHop, bestDstNode, (void *) bestSrcNode);
		pathsCost[bestDstNode] = bestCost;
		arraySet(numberOfHops, bestDstNode, (void *) arrayGet(numberOfHops, bestSrcNode) + 1);

		if (destination == bestDstNode) break ;
	}

	if (destination == -1) {

		/*
		 * TODO: assemble the complete routing table.
		 */
	
		listFree(defList);
		setFree(defSet);
		arrayFree(lastHop);
		arrayFree(numberOfHops);
		free(pathsCost);
		free(defList);
		free(defSet);
		free(lastHop);
		free(numberOfHops);

		return(0.0);
	}
	else {

		hopsForNode = (unsigned long) arrayGet(numberOfHops, destination);
		* output = arrayNew(hopsForNode + 1);
		node = destination;
		for (i = hopsForNode; i; i--) {

			arraySet(* output, i, (void *) node);
			node = (unsigned long) arrayGet(lastHop, node);
		}
		arraySet(* output, 0, (void *) source);

		bestCost = pathsCost[destination];

		listFree(defList);
		setFree(defSet);
		arrayFree(lastHop);
		arrayFree(numberOfHops);
		free(pathsCost);
		free(defList);
		free(defSet);
		free(lastHop);
		free(numberOfHops);

		return(bestCost);

	}
	
}

