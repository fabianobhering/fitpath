#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define _ISOC99_SOURCE
#include <math.h>

#include "parser.h"
#include "graph.h"
#include "yen.h"
#include "prefixTree.h"
#include "list.h"
#include "array.h"
#include "simulation.h"
#include "prefixTree.h"


#include "heuristics.h"
#include "dijkstra.h"
#include "floyd.h"

#include "memory.h"

typedef struct {

	t_array * pool;
	int maxVoted;
	int maxEnabled;
} t_recursion_item;

#define ABS(x)	(((x) >= 0) ? (x) : -(x))

int heuristic3GenerateCodingCandidates(t_graph * graph, t_array * currentPaths, int numberOfFlows, t_floyd_solution floydSolution) {

	int i, j, k, l;
	int node1, node2;
	int hopsPrefix, hopsSuffix;
	t_array * path1, * path2;
	t_array * prefix, * suffix;
	int path1Len, path2Len;
	int bestSrc, bestDst, bestHops, bestMiddle, bestFlow1, bestFlow2;
	float bestCost, target, bestDiff, costToMiddle;
	int suffixStart;

printf("Initial candidate set:\n");
for (k = 0; k < arrayLength(currentPaths); k++) {

	for (j = 0; j < arrayLength(arrayGet(currentPaths, k)); j++) {

		printf("%lu ", (unsigned long) arrayGet(arrayGet(currentPaths, k), j));
	}
	printf("\n");
}
	bestCost = GRAPH_INFINITY;
	for (i = 0; i < numberOfFlows; i++) {

		path1 = arrayGet(currentPaths, i);
		path1Len = arrayLength(path1);
		for (j = i + 1; j < numberOfFlows; j++) {

			path2 = arrayGet(currentPaths, j);
			path2Len = arrayLength(path2);
			for (k = 1; k < path1Len - 1; k++) {

				for (l = 1; l < path2Len - 1; l++) {

					node1 = (long) arrayGet(path1, k);
					node2 = (long) arrayGet(path2, l);
					if (bestCost > floydGetCost(floydSolution, node1, node2)) {

						if (floydPathLength(floydSolution, node1, node2) == 0) continue ;

						bestSrc = node1;
						bestDst = node2;
						bestHops = floydPathLength(floydSolution, node1, node2);
						bestCost = floydGetCost(floydSolution, node1, node2);
						bestFlow1 = i;
						bestFlow2 = j;
					}
				}
			}
		}
	}
printf("Bestcost = %f, bestSrc = %d, bestDst = %d\n", bestCost, bestSrc, bestDst);
	if (bestCost == GRAPH_INFINITY) return(0);
	
	bestMiddle = bestSrc;
	target = bestCost / 2.0;
	bestDiff = target;

	for (i = 1; i <= bestHops; i++) {

		node2 = floydPathHop(floydSolution, bestSrc, bestDst, i);
		costToMiddle = floydGetCost(floydSolution, bestSrc, node2);
		if (ABS(target - costToMiddle) < bestDiff) {

			bestDiff = ABS(target - costToMiddle);
			bestMiddle = node2;
		}
		if (costToMiddle > target) break ;
	}
printf("BestMiddle = %d\n", bestMiddle);
	path1 = arrayGet(currentPaths, bestFlow1);

	if (bestMiddle != (long) arrayGet(path1, arrayLength(path1) - 1) && bestMiddle != (long) arrayGet(path1, 0)) {

		suffix = floydGetPath(floydSolution, bestMiddle, (long) arrayGet(path1, arrayLength(path1) - 1));
		prefix = floydGetPath(floydSolution, (long) arrayGet(path1, 0), bestMiddle);
		hopsSuffix = arrayLength(suffix);
		hopsPrefix = arrayLength(prefix);
/*		
printf("sufix\n");
for (j = 0; j < arrayLength(suffix); j++) {

	printf("%lu ", (unsigned long) arrayGet(suffix, j));
}
printf("\n");
printf("prefix\n");
for (j = 0; j < arrayLength(prefix); j++) {

	printf("%lu ", (unsigned long) arrayGet(prefix, j));
}
printf("\n");
*/
		suffixStart = 0;
		for (i = 0; i < hopsPrefix; i++) {

			node2 = (long) arrayGet(prefix, i);
			for (j = 0; j < hopsSuffix; j++) if ((long) arrayGet(suffix, j) == node2) break ;
			if (j < hopsSuffix) {

				suffixStart = j + 1;
				hopsPrefix = i + 1;
				break ;
			}
		}
		path2 = arrayNew(hopsPrefix + (hopsSuffix - suffixStart));
		for (i = 0; i < hopsPrefix; i++) arraySet(path2, i, arrayGet(prefix, i));
		for (i = suffixStart; i < hopsSuffix; i++) arraySet(path2, i + hopsPrefix - suffixStart, arrayGet(suffix, i));

		arrayFree(path1);
		free(path1);
		arrayFree(prefix);
		free(prefix);
		arrayFree(suffix);
		free(suffix);
		arraySet(currentPaths, bestFlow1, path2);
	}

	path1 = arrayGet(currentPaths, bestFlow2);

	if (bestMiddle != (long) arrayGet(path1, arrayLength(path1) - 1) && bestMiddle != (long) arrayGet(path1, 0)) {

		suffix = floydGetPath(floydSolution, bestMiddle, (long) arrayGet(path1, arrayLength(path1) - 1));
		prefix = floydGetPath(floydSolution, (long) arrayGet(path1, 0), bestMiddle);
		hopsSuffix = arrayLength(suffix);
		hopsPrefix = arrayLength(prefix);
/*		
printf("sufix\n");
for (j = 0; j < arrayLength(suffix); j++) {

	printf("%lu ", (unsigned long) arrayGet(suffix, j));
}
printf("\n");
printf("prefix\n");
for (j = 0; j < arrayLength(prefix); j++) {

	printf("%lu ", (unsigned long) arrayGet(prefix, j));
}
printf("\n");
*/
		suffixStart = 0;
		for (i = 0; i < hopsPrefix; i++) {

			node2 = (long) arrayGet(prefix, i);
			for (j = 0; j < hopsSuffix; j++) if ((long) arrayGet(suffix, j) == node2) break ;
			if (j < hopsSuffix) {

				suffixStart = j + 1;
				hopsPrefix = i + 1;
				break ;
			}
		}
		path2 = arrayNew(hopsPrefix + (hopsSuffix - suffixStart));
		for (i = 0; i < hopsPrefix; i++) arraySet(path2, i, arrayGet(prefix, i));
		for (i = suffixStart; i < hopsSuffix; i++) arraySet(path2, i + hopsPrefix - suffixStart, arrayGet(suffix, i));

		arrayFree(path1);
		free(path1);
		arrayFree(prefix);
		free(prefix);
		arrayFree(suffix);
		free(suffix);
		arraySet(currentPaths, bestFlow2, path2);
	}

printf("Found candidate set(coding):\n");
for (k = 0; k < arrayLength(currentPaths); k++) {

	for (j = 0; j < arrayLength(arrayGet(currentPaths, k)); j++) {

		printf("%lu ", (unsigned long) arrayGet(arrayGet(currentPaths, k), j));
	}
	printf("\n");
}

	return(1);
}

int main(int argc, char ** argv) {

	t_graph * graph;
	int * currentSrc, * currentDst;
	int i, j, k, l, f;
	t_array * currentPaths, * bestPaths, * path, * newPath, * linearPath;
	t_list * src, * dst;
	t_array * asrc, * adst;
	float currentCost, bestCost;
	int numberOfFlows, numberOfNodes, numberOfLoops = 10;
	t_array * recursionStack;
	t_recursion_item * recursionItem, * recursionItem2;
	int currentFlow, prevFlow;
	int value;
	int newSrc, newDst;
	int min;
	int totalHops;
	int primes[] = {1, 2, 3, 5, 7, 11, 13, 17, 23, 29, 31};
	int numberOfPrimes = 11;
	t_floyd_solution floydSolution;
	t_prefixTreeNode * root;
	t_list * candidates, * candidates2;
	t_prefixTreeNode * candidate;

	if (argc != 2) {

		fprintf(stderr, "Use: %s <input>\n", argv[0]);
		exit(1);
	}

	graph = parserParse(argv[1], & src, & dst);

	floydSolution = floydSolve(graph);
	numberOfNodes = graphSize(graph);

	/*
	 * Build the initial solution.
	 */

	numberOfFlows = listLength(src);
	asrc = arrayNew(numberOfFlows);
	adst = arrayNew(numberOfFlows);

	totalHops = 0;
	currentPaths = arrayNew(numberOfFlows);
	for (i = 0; i < numberOfFlows; i++) {

		currentSrc = listCurrent(src);
		currentDst = listCurrent(dst);

		arraySet(asrc, i, (void *) (long) * currentSrc);
		arraySet(adst, i, (void *) (long) * currentDst);

		if (dijkstra(graph, * currentSrc, * currentDst, & path) == GRAPH_INFINITY) {

			fprintf(stderr, "Graph is not connected!\n");
			exit(1);
		}

		arraySet(currentPaths, i, path);

		listNext(src);
		listNext(dst);

		totalHops += arrayLength(path);
	}

	listFreeWithData(src);
	free(src);
	listFreeWithData(dst);
	free(dst);

	root = prefixTreeNew((long) arrayGet(asrc, 0));
	candidates = listNew();
	candidates2 = listNew();
	linearPath = arrayNew(totalHops);
	k = 0;
	for (i = 0; i < numberOfFlows; i++) {

		path = arrayGet(currentPaths, i);
		for (j = 0; j < arrayLength(path); j++) {

			arraySet(linearPath, k++, arrayGet(path, j));
		}

		arrayFree(path);
		free(path);
		arraySet(currentPaths, i, NULL);
	}
	candidate = prefixTreeInsert(root, linearPath, graph);
	listAdd(candidates, candidate);
	arrayFree(linearPath);
	free(linearPath);

	recursionStack = arrayNew(numberOfFlows);
	for (i = 0; i < numberOfFlows; i++) {

		recursionItem = malloc(sizeof(t_recursion_item));
		recursionItem->pool = arrayNew(numberOfNodes);
		arraySet(recursionStack, i, recursionItem);
	}

	while(numberOfLoops--) {

		for (l = 0; l < numberOfPrimes; l++) {

			if (primes[l] >= numberOfFlows) break ;
			if (numberOfFlows % primes[l] == 0 && primes[l] != 1) continue ;
//printf("Current prime = %d\n", primes[l]);
			for (i = 0; i < numberOfFlows; i++) {

				totalHops = 0;
//printf("Restarting recursion.\n");
				currentFlow = i;

				recursionItem = arrayGet(recursionStack, i);
				arrayClear(recursionItem->pool);
				recursionItem->maxEnabled = 0;
				recursionItem->maxVoted = -1;
				path = floydGetPath(floydSolution, (long) arrayGet(asrc, currentFlow), (long) arrayGet(adst, currentFlow));
				arraySet(currentPaths, currentFlow, path);
//printf("Reenabling all nodes.\n");
				graphReenableAll(graph);
				totalHops += arrayLength(path);

//printf("Found path for flow %d:\n", currentFlow);
				for (j = 0; j < arrayLength(path); j++) {

//printf("%ld ", (long) arrayGet(path, j));
					for (k = 0; k < numberOfNodes; k++) {
//printf("Trying %d vs %d\n", (long) arrayGet(path, j), k);
						if (graphGetCost(graph, k, (long) arrayGet(path, j)) < GRAPH_INFINITY) {

							arrayInc(recursionItem->pool, k);
							value = (long) arrayGet(recursionItem->pool, k);
							if (value > recursionItem->maxVoted) recursionItem->maxVoted = value;
							if (value == 1) {
//printf("Disabling node %d\n", k);
								graphDisableNode(graph, k);
							}
						}
					}
				}
//printf("\n");

//for (k = 0; k < numberOfNodes; k++) {
//printf("pool[%d]: %d\n", k, (long) arrayGet(recursionItem->pool, k))							;
//}
				currentFlow = (currentFlow + primes[l]) % numberOfFlows;
				recursionItem = arrayGet(recursionStack, currentFlow);
				recursionItem->maxEnabled = 0;
				recursionItem->maxVoted = -1;

				while(1) {

					prevFlow = currentFlow - primes[l];
					if (prevFlow < 0) prevFlow = numberOfFlows + prevFlow;
					recursionItem = arrayGet(recursionStack, currentFlow);

//printf("Starting recursion level %d\n", currentFlow);
					if (recursionItem->maxVoted == -1) {

						recursionItem2 = (t_recursion_item *) arrayGet(recursionStack, prevFlow);
						arrayCopy(recursionItem->pool, recursionItem2->pool);

						newSrc = (long) arrayGet(asrc, currentFlow);
						newDst = (long) arrayGet(adst, currentFlow);
						
						if (dijkstra(graph, newSrc, newDst, & path) == GRAPH_INFINITY) {
//printf("Couldn't find a path. Backtrack.\n");
							currentFlow = prevFlow;
							continue ;
						}

						totalHops += arrayLength(path);

						if (arrayGet(currentPaths, currentFlow)) {

							arrayFree(arrayGet(currentPaths, currentFlow));
							free(arrayGet(currentPaths, currentFlow));
						}
						arraySet(currentPaths, currentFlow, path);
//printf("Found path for flow %d:\n", currentFlow);
						for (j = 0; j < arrayLength(path); j++) {
//printf("%ld ", (long) arrayGet(path, j));
							for (k = 0; k < numberOfNodes; k++) {

								if (graphGetCost(graph, k, (long) arrayGet(path, j)) < GRAPH_INFINITY) {

									arrayInc(recursionItem->pool, k);
									value = (long) arrayGet(recursionItem->pool, k);
									if (value > recursionItem->maxVoted) recursionItem->maxVoted = value;
								}
							}
						}
//printf("\n");

//for (k = 0; k < numberOfNodes; k++) {
//printf("pool[%d]: %d\n", k, (long) arrayGet(recursionItem->pool, k))							;
//}
						currentFlow = (currentFlow + primes[l]) % numberOfFlows;

						if (currentFlow == i) {
/*
printf("Found candidate set:\n");
for (k = 0; k < arrayLength(currentPaths); k++) {

	for (j = 0; j < arrayLength(arrayGet(currentPaths, k)); j++) {
		
		printf("%lu ", (unsigned long) arrayGet(arrayGet(currentPaths, k), j));
	}
	printf("\n");
}
*/
							linearPath = arrayNew(totalHops);
							k = 0;
							for (f = 0; f < numberOfFlows; f++) {

								path = arrayGet(currentPaths, f);
								for (j = 0; j < arrayLength(path); j++) {

									arraySet(linearPath, k++, arrayGet(path, j));
								}

								arrayFree(path);
								free(path);
								arraySet(currentPaths, f, NULL);
							}
							candidate = prefixTreeInsert(root, linearPath, graph);
							if (candidate) listAdd(candidates, candidate);
							arrayFree(linearPath);
							free(linearPath);

							break ;
						}

//printf("Reenabling all nodes.\n");
						graphReenableAll(graph);
						for (k = 0; k < numberOfNodes; k++) {

							if ((long) arrayGet(recursionItem->pool, k) > recursionItem->maxEnabled) {
//printf("Disabling node %d\n", k);
							
								graphDisableNode(graph, k);
							}
						} 

						recursionItem = arrayGet(recursionStack, currentFlow);
						recursionItem->maxEnabled = 0;
						recursionItem->maxVoted = -1;
					}
					else {

						if (recursionItem->maxEnabled == recursionItem->maxVoted) {
//printf("No more reenable to do. Backtrack.\n");
							totalHops -= arrayLength(arrayGet(currentPaths, currentFlow));
							if (totalHops == 0) break ;
							currentFlow = prevFlow;
							continue ;
						}

						min = recursionItem->maxVoted;
						for (k = 0; k < numberOfNodes; k++) {
						
							if ((long) arrayGet(recursionItem->pool, k) <= recursionItem->maxEnabled) continue ;
							if ((long) arrayGet(recursionItem->pool, k) < min) min = (long) arrayGet(recursionItem->pool, k);
						}
//printf("Enabling all nodes till %d votes\n", min)					;
						recursionItem->maxEnabled = min;
//printf("Reenabling all nodes.\n");
						graphReenableAll(graph);
						for (k = 0; k < numberOfNodes; k++) {

							if ((long) arrayGet(recursionItem->pool, k) > recursionItem->maxEnabled) {
//printf("Disabling node %d\n", k);
							
								graphDisableNode(graph, k);
							}
						} 

						currentFlow = (currentFlow + primes[l]) % numberOfFlows;
						recursionItem = arrayGet(recursionStack, currentFlow);
						recursionItem->maxEnabled = 0;
						recursionItem->maxVoted = -1;
					}
				}
			}
		}
	}

//printf("Found %d candidates.\n", listLength(candidates));

	bestCost = GRAPH_INFINITY;
	bestPaths = arrayNew(numberOfFlows);
	arrayClear(bestPaths);
	for (candidate = listBegin(candidates); candidate; candidate = listNext(candidates)) {

		linearPath = prefixTreePath(candidate);
		k = 0;
		l = 0;
		for (i = 0; i < numberOfFlows; i++) {

			while(1) {
			
				if ((long) arrayGet(linearPath, k++) == (long) arrayGet(adst, i)) break ;
			}

			newPath = arrayNew(k - l);
			for (j = l; j < k; j++) arraySet(newPath, j - l, arrayGet(linearPath, j));
			l = k;

			path = arrayGet(currentPaths, i);
			if (path) {

				arrayFree(path);
				free(path);
			}
			arraySet(currentPaths, i, newPath);
		}

printf("Trying candidate set:\n");
for (i = 0; i < arrayLength(currentPaths); i++) {

	for (j = 0; j < arrayLength(arrayGet(currentPaths, i)); j++) {
		
		printf("%lu ", (unsigned long) arrayGet(arrayGet(currentPaths, i), j));
	}
	printf("\n");
}
//fprintf(stderr, "___currentCost = %f\n", currentCost);
		currentCost = simulationSimulate(graph, currentPaths);
//fprintf(stderr, "___currentCost = %f\n", currentCost);
		if (currentCost < bestCost) {

			for (i = 0; i < numberOfFlows; i++) {

				path = arrayGet(bestPaths, i);
				if (path) {

					arrayFree(path);
					free(path);
				}
				path = arrayGet(currentPaths, i);
				newPath = arrayNew(arrayLength(path));
				arrayCopy(newPath, path);
				arraySet(bestPaths, i, newPath);
			}

			bestCost = currentCost;
		}
#ifdef NOCODING
		for (j = 0; j < 3; j++) {

			if (heuristic3GenerateCodingCandidates(graph, currentPaths, numberOfFlows, floydSolution)) {

				totalHops = 0;
				for (f = 0; f < numberOfFlows; f++) {

					path = arrayGet(currentPaths, f);
					totalHops += arrayLength(path);
				}

				linearPath = arrayNew(totalHops);
				k = 0;
				for (f = 0; f < numberOfFlows; f++) {

					path = arrayGet(currentPaths, f);
					for (l = 0; l < arrayLength(path); l++) {

						arraySet(linearPath, k++, arrayGet(path, l));
					}
				}
				candidate = prefixTreeInsert(root, linearPath, graph);
				if (candidate) listAdd(candidates2, candidate);
				else {

					arrayFree(linearPath);
					free(linearPath);
					break ;
				}

				arrayFree(linearPath);
				free(linearPath);

				currentCost = simulationSimulate(graph, currentPaths);
				if (currentCost == GRAPH_INFINITY) break ;
				if (currentCost < bestCost) {

					for (i = 0; i < numberOfFlows; i++) {

						path = arrayGet(bestPaths, i);
						if (path) {

							arrayFree(path);
							free(path);
						}
						path = arrayGet(currentPaths, i);
						newPath = arrayNew(arrayLength(path));
						arrayCopy(newPath, path);
						arraySet(bestPaths, i, newPath);
					}

					bestCost = currentCost;
				}
			}
		}
#endif
	}

	printf("Best path set has cost %.2f and is:\n", bestCost / GRAPH_MULTIPLIER);
	for (i = 0; i < arrayLength(bestPaths); i++) {

		for (j = 0; j < arrayLength(arrayGet(bestPaths, i)); j++) {
			
			printf("%lu ", (unsigned long) arrayGet(arrayGet(bestPaths, i), j));
		}
		printf("\n");
	}

	for (i = 0; i < arrayLength(bestPaths); i++) {
		
		arrayFree(arrayGet(bestPaths, i));
		free(arrayGet(bestPaths, i));
		arrayFree(arrayGet(currentPaths, i));
		free(arrayGet(currentPaths, i));
	}
	arrayFree(bestPaths);
	free(bestPaths);
	arrayFree(currentPaths);
	free(currentPaths);

	for (i = 0; i < numberOfFlows; i++) {

		recursionItem = arrayGet(recursionStack, i);
		arrayFree(recursionItem->pool);
		free(recursionItem->pool);
		free(recursionItem);
	}

	arrayFree(recursionStack);
	free(recursionStack);

	floydFree(floydSolution, graph);

	graphFree(graph);
	free(graph);

	arrayFree(asrc);
	free(asrc);
	arrayFree(adst);
	free(adst);

	for (candidate = listBegin(candidates); candidate; candidate = listNext(candidates)) {

		prefixTreePrune(candidate);
	}

	listFree(candidates);
	free(candidates);

	for (candidate = listBegin(candidates2); candidate; candidate = listNext(candidates2)) {

		prefixTreePrune(candidate);
	}

	listFree(candidates2);
	free(candidates2);

	return(0);
}

