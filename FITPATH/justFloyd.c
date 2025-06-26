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
	t_array * currentPaths;
	int numberOfPathsPerFlow = 5;

	if (argc != 3) {

		fprintf(stderr, "Use: %s <input> <npathsPerFlow>\n", argv[0]);
		exit(1);
	}

	numberOfPathsPerFlow = atoi(argv[2]);
	graph = parserParse(argv[1], & src, & dst);

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
		pathList = yen(graph, * currentSrc, * currentDst, numberOfPathsPerFlow);
		arraySet(nodePairs, i, pathList);

//printf("%d paths were generated between nodes %d and %d:\n\n", listLength(pathList), * currentSrc, * currentDst);
//for (path = listBegin(pathList); path; path = listNext(pathList)) prefixTreePrint(path);

		i++;
		if (listNext(src) == NULL) break ;
		listNext(dst);
	}

	numberOfPairs = i;

	/*
	 * Now comes the expensive part: we generate all combinations 
	 * of paths and simulate them, storing the best combination as
	 * we go.
	 */
	currentPaths = arrayNew(numberOfPairs);

	for (i = 0; i < numberOfPairs; i++) {

		path = listBegin(arrayGet(nodePairs, i));
		arraySet(currentPaths, i, prefixTreePath(path));
	}

	run = 1;
	while(run) {

		printf("Paths\n");
		for (i = 0; i < arrayLength(currentPaths); i++) {

			for (j = 0; j < arrayLength(arrayGet(currentPaths, i)); j++) {
				
				printf("%lu ", (unsigned long) arrayGet(arrayGet(currentPaths, i), j));
			}
			printf("\n");
		}
		for (i = 0; i < numberOfPairs; i++) {

			path = listNext(arrayGet(nodePairs, i));
			if (path == NULL) {

				if (i == numberOfPairs - 1) {
					run = 0;
					break ;
				}

				path = listBegin(arrayGet(nodePairs, i));
				arraySet(currentPaths, i, prefixTreePath(path));
				continue ;
			}
			arraySet(currentPaths, i, prefixTreePath(path));
			break ;
		}
	}

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

