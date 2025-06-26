#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define _ISOC99_SOURCE
#include <math.h>

#include "parser.h"
#include "graph.h"
#include "paths.h"
#include "prefixTree.h"
#include "list.h"
#include "array.h"
#include "simulation.h"

#include "heuristics.h"
#include "dijkstra.h"

int main(int argc, char ** argv) {

	int * currentSrc, * currentDst;
	int i, j, numberOfPairs, run;
	t_graph * graph;
	t_list * pathList;
	t_list * src, * dst;
	t_prefixTreeNode * path;
	t_array * nodePairs;
	t_array * currentPaths, * bestPaths;
	float currentCost, bestCost, upperBound;

	if (argc != 2) {

		fprintf(stderr, "Use: %s <input>\n", argv[0]);
		exit(1);
	}

	graph = parserParse(argv[1], & src, & dst);
//graphPrint(graph);
//

	/*
	 * Compute paths and place them in an array.
	 * Each array entry corresponds to a pair of
	 * source and destination. Each entry is a 
	 * handler for a list containing the paths.
	 */
	nodePairs = arrayNew(listLength(src));

	i = 0;
	listBegin(src);
	listBegin(dst);
	while(1) {

		currentSrc = listCurrent(src);
		currentDst = listCurrent(dst);
		pathList = pathsBuild(graph, * currentSrc, * currentDst);
		arraySet(nodePairs, i, pathList);

printf("%d paths were generated between nodes %d and %d:\n\n", listLength(pathList), * currentSrc, * currentDst);
//for (path = listBegin(pathList); path; path = listNext(pathList)) prefixTreePrint(path);
//exit(0);
		i++;
		if (listNext(src) == NULL) break ;
		listNext(dst);
	}

	numberOfPairs = i;

bestPaths = arrayNew(numberOfPairs);
listBegin(src);
listBegin(dst);
i = 0;
while(1) {
	t_array * shortestpath;

	currentSrc = listCurrent(src);
	currentDst = listCurrent(dst);
	dijkstra(graph, * currentSrc, * currentDst, & shortestpath);
	arraySet(bestPaths, i++, shortestpath);

	if (listNext(src) == NULL) break ;
	listNext(dst);
}
bestCost = simulationSimulate(graph, bestPaths);
printf("initial guest of cost is %f\n", bestCost);
	/*
	 * Now comes the expensive part: we generate all combinations 
	 * of paths and simulate them, storing the best combination as
	 * we go.
	 */
	currentPaths = arrayNew(numberOfPairs);
	upperBound = 0.0;
//	bestCost = INFINITY;

	for (i = 0; i < numberOfPairs; i++) {

		path = listBegin(arrayGet(nodePairs, i));
		arraySet(currentPaths, i, prefixTreePath(path));
		upperBound += prefixTreeGetUpperBound(path);
	}
heuristicInit(graphSize(graph), numberOfPairs);
	run = 1;
	while(run) {
		currentCost = simulationSimulate(graph, currentPaths);
#ifdef COMMENT
printf("Trying path set:\n");
for (i = 0; i < arrayLength(currentPaths); i++) {

	for (j = 0; j < arrayLength(arrayGet(currentPaths, i)); j++) {
		
		printf("%lu ", (unsigned long) arrayGet(arrayGet(currentPaths, i), j));
	}
	printf("\n");
}

printf("__: %e vs. %e vs. %e vs. %e\n", 1.0/upperBound, currentCost, heuristicMultiplePathInterFlowLowerUpperBound(graph, currentPaths),
heuristicMultiplePathNewlowerBound(graph, currentPaths));
#endif
		if (currentCost < bestCost) {

			for (i = 0; i < numberOfPairs; i++) {

				arraySet(bestPaths, i, arrayGet(currentPaths, i));
			}
			bestCost = currentCost;
		}

		while(1) {

			for (i = 0; i < numberOfPairs; i++) {

if (i > 0) {
printf("Currently at: (");
int z;
for (z = 0; z < numberOfPairs - 1; z++) printf("%d,", listGetIndex(arrayGet(nodePairs, z)));
printf("%d", listGetIndex(arrayGet(nodePairs, z)));
printf(")\n");
}
				path = listNext(arrayGet(nodePairs, i));
				if (path == NULL) {

					if (i == numberOfPairs - 1) {
						run = 0;
						break ;
					}

					path = listBegin(arrayGet(nodePairs, i));
					arraySet(currentPaths, i, prefixTreePath(path));
					upperBound += prefixTreeGetUpperBound(path);
					continue ;
				}
				arraySet(currentPaths, i, prefixTreePath(path));
				break ;
			}
			if (run == 0) break ;
//			if (heuristicMultiplePathNewlowerBound(graph, currentPaths) < bestCost) break ;
break ;
		}
	}

	printf("Best path set has cost %.2f and is:\n", bestCost / GRAPH_MULTIPLIER);
	for (i = 0; i < arrayLength(bestPaths); i++) {

		for (j = 0; j < arrayLength(arrayGet(bestPaths, i)); j++) {
			
			printf("%lu ", (unsigned long) arrayGet(arrayGet(bestPaths, i), j));
		}
		printf("\n");
	}

//printf("Tested %llu paths\n", testedPaths);
#ifdef COMMENT2
t_array * tmp = arrayNew(2);
arraySet(currentPaths, 0, tmp);
arraySet(arrayGet(currentPaths, 0), 0, 0); 
arraySet(arrayGet(currentPaths, 0), 1, 9);
tmp = arrayNew(5);
arraySet(currentPaths, 1, tmp);
arraySet(arrayGet(currentPaths, 1), 0, 5); 
arraySet(arrayGet(currentPaths, 1), 1, 9);
arraySet(arrayGet(currentPaths, 1), 2, 0);
arraySet(arrayGet(currentPaths, 1), 3, 3);
arraySet(arrayGet(currentPaths, 1), 4, 8);
tmp = arrayNew(5);
arraySet(currentPaths, 2, tmp);
arraySet(arrayGet(currentPaths, 2), 0, 1); 
arraySet(arrayGet(currentPaths, 2), 1, 4);
arraySet(arrayGet(currentPaths, 2), 2, 9);
arraySet(arrayGet(currentPaths, 2), 3, 0);
arraySet(arrayGet(currentPaths, 2), 4, 11);
printf("Path cost is %.2f\n", simulationSimulate(graph, currentPaths));
#endif

	arrayFree(bestPaths);
	free(bestPaths);
	arrayFree(currentPaths);
	free(currentPaths);

	/*
	 * Free everything.
	 */

	listFreeWithData(dst);
	free(dst);
	listFreeWithData(src);
	free(src);
	graphFree(graph);
	free(graph);

	for (i = 0; i < numberOfPairs; i++) {

		pathList = arrayGet(nodePairs, i);
		for (path = listBegin(pathList); path; path = listNext(pathList)) prefixTreePrune(path);
		listFree(pathList);
		free(pathList);
	}

	arrayFree(nodePairs);
	free(nodePairs);

	return(0);
}

