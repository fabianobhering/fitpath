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

#include <string.h>

int main(int argc, char ** argv) {

	int * currentSrc, * currentDst;
	int i, j, numberOfPairs, run;
unsigned long asd;
	t_graph * graph;
	t_list * pathList;
	t_list * src, * dst;
	t_prefixTreeNode * path;
	t_array * nodePairs;
	t_array * currentPaths, * bestPaths;
	float * currentSimulatedCosts;
	float currentCost, bestCost, upperBound;
	char filename[800], * inputPrefix, * input;

	if (argc != 2 && argc != 4) {

		fprintf(stderr, "Use: %s [-p inputPrefix] <input>\n", argv[0]);
		exit(1);
	}

	input = argv[argc - 1];

	if (argc > 2 && !strcmp(argv[1], "-p"))
		inputPrefix = argv[2];
	else 
		inputPrefix = input;

	graph = parserParse(input, & src, & dst);

	/*
	 * Load each list of paths (for each pair) 
	 * from precomputed file.
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

		sprintf(filename, "%s_%d_%d.sav", inputPrefix, * currentSrc, * currentDst);
		pathList = prefixTreeLoad(filename);
		arraySet(nodePairs, i, pathList);

printf("%d paths were generated between nodes %d and %d:\n\n", listLength(pathList), * currentSrc, * currentDst);
//for (path = listBegin(pathList); path; path = listNext(pathList)) prefixTreePrint(path);
		i++;
		if (listNext(src) == NULL) break ;
		listNext(dst);
	}

	numberOfPairs = i;

	currentSimulatedCosts = malloc(numberOfPairs * sizeof(float));

	/*
	 * Now comes the expensive part: we generate all combinations 
	 * of paths and simulate them, storing the best combination as
	 * we go.
	 */
	currentPaths = arrayNew(numberOfPairs);
	bestPaths = arrayNew(numberOfPairs);
	bestCost = INFINITY;
	upperBound = 0.0;

	for (i = 0; i < numberOfPairs; i++) {

		path = listBegin(arrayGet(nodePairs, i));
		arraySet(currentPaths, i, prefixTreePath(path));
		currentSimulatedCosts[i] = prefixTreeGetSimulatedCost(path);
		upperBound += 1.0/prefixTreeGetSimulatedCost(path);
	}

	heuristicInit(graphSize(graph), numberOfPairs);
asd = 0;
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
asd++;
printf("__: %e vs %e\n", heuristicMultiplePathNewlowerBound(graph, currentPaths, currentSimulatedCosts, bestCost), bestCost);
#endif

		if (currentCost < bestCost) {

			for (i = 0; i < numberOfPairs; i++) {

				arraySet(bestPaths, i, arrayGet(currentPaths, i));
			}
			bestCost = currentCost;
		}

		while(1) {

			for (i = 0; i < numberOfPairs; i++) {
/*
if (i > 0) {
printf("Currently at: (");
int z;
for (z = 0; z < numberOfPairs - 1; z++) printf("%d,", listGetIndex(arrayGet(nodePairs, z)));
printf("%d", listGetIndex(arrayGet(nodePairs, z)));
printf(")\n");
}*/
				upperBound -= 1.0/prefixTreeGetSimulatedCost(listCurrent(arrayGet(nodePairs, i)));
				path = listNext(arrayGet(nodePairs, i));
				if (path == NULL) {

					if (i == numberOfPairs - 1) {
						run = 0;
						break ;
					}

					path = listBegin(arrayGet(nodePairs, i));
					arraySet(currentPaths, i, prefixTreePath(path));
					currentSimulatedCosts[i] = prefixTreeGetSimulatedCost(path);
					upperBound += 1.0/prefixTreeGetSimulatedCost(path);
					continue ;
				}
				upperBound += 1.0/prefixTreeGetSimulatedCost(path);
//printf("Upper bound is %.2f vs. best cost of %.2f\n", 1.0/upperBound, bestCost);
				if (1.0/upperBound > bestCost) {

					upperBound -= 1.0/prefixTreeGetSimulatedCost(path);

					if (i == numberOfPairs - 1) {
						run = 0;
						break ;
					}

					path = listBegin(arrayGet(nodePairs, i));
					arraySet(currentPaths, i, prefixTreePath(path));
					currentSimulatedCosts[i] = prefixTreeGetSimulatedCost(path);
					upperBound += 1.0/prefixTreeGetSimulatedCost(path);
					continue ;
				}

				arraySet(currentPaths, i, prefixTreePath(path));
				currentSimulatedCosts[i] = prefixTreeGetSimulatedCost(path);
				break ;
			}
			if (run == 0) break ;
/*			
printf("Should we try path set:\n");
for (i = 0; i < arrayLength(currentPaths); i++) {

	for (j = 0; j < arrayLength(arrayGet(currentPaths, i)); j++) {
		
		printf("%lu ", (unsigned long) arrayGet(arrayGet(currentPaths, i), j));
	}
	printf("\n");
}
*/
//printf("Heuristic cost is given by %.2f\n", heuristicMultiplePathNewlowerBound(graph, currentPaths, bestCost));

			if (heuristicMultiplePathNewlowerBound(graph, currentPaths, currentSimulatedCosts, bestCost) < bestCost) break ;
//break ;
		}
	}

	printf("Best path set has cost %.2f and is:\n", bestCost / GRAPH_MULTIPLIER);
	for (i = 0; i < arrayLength(bestPaths); i++) {

		for (j = 0; j < arrayLength(arrayGet(bestPaths, i)); j++) {
			
			printf("%lu ", (unsigned long) arrayGet(arrayGet(bestPaths, i), j));
		}
		printf("\n");
	}

	/*
	 * Free everything.
	 */

	arrayFree(bestPaths);
	free(bestPaths);
	arrayFree(currentPaths);
	free(currentPaths);

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

