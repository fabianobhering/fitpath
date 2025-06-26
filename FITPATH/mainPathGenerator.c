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
#include "floatHeap.h"

#include "heuristics.h"
#include "dijkstra.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

int main(int argc, char ** argv) {

	int i, j, numberOfNodes;
	t_graph * graph;
	t_list * pathList;
	t_list * src, * dst;
	t_prefixTreeNode * path;
	t_array * currentPaths;
	float currentCost;
	t_floatHeap * heap;
	char filename[800], * input, * outputPrefix;
	int starti, startj;
	int numberOfProcs;
	int pid = 0;

	if (argc < 2) {

		fprintf(stderr, "Use: %s [-n <numberOfProcs>] [-i <start_i>] [-j <start_j>] [-p <outputPrefix>] <input>\n", argv[0]);
		exit(1);
	}


	input = argv[argc - 1];
	starti = 0;
	startj = 0;
	numberOfProcs = 1;
	outputPrefix = input;

	i = 1;
	while(i < argc - 1) {

		if (!strcmp(argv[i], "-n")) {

			if (i == argc - 2) {

				fprintf(stderr, "Option '-n' requires an argument.\n");
				exit(1);
			}

			numberOfProcs = atoi(argv[i + 1]);
			i += 2;
			continue ;
		}

		if (!strcmp(argv[i], "-i")) {

			if (i == argc - 2) {

				fprintf(stderr, "Option '-i' requires an argument.\n");
				exit(1);
			}

			starti = atoi(argv[i + 1]);
			i += 2;
			continue ;
		}

		if (!strcmp(argv[i], "-j")) {

			if (i == argc - 2) {

				fprintf(stderr, "Option '-j' requires an argument.\n");
				exit(1);
			}

			startj = atoi(argv[i + 1]);
			i += 2;
			continue ;
		}

		if (!strcmp(argv[i], "-p")) {

			if (i == argc - 2) {

				fprintf(stderr, "Option '-p' requires an argument.\n");
				exit(1);
			}

			outputPrefix = argv[i + 1];
			i += 2;
			continue ;
		}

		fprintf(stderr, "Unknown option: '%s'.\n", argv[i]);
		exit(1);
	}

	graph = parserParse(input, & src, & dst);
	numberOfNodes = graphSize(graph);
	currentPaths = arrayNew(1);
	heap = floatHeapNew();

	for (i = 0; i < numberOfProcs; i++) {

		pid = fork();
		if (pid == 0) break ;
		starti++;
		startj = 0;
		if (starti == numberOfNodes) break ;
	}

	if (pid != 0) {

		while(starti < numberOfNodes) {

			wait(NULL);
			pid = fork();
			if (pid == 0) break ;
			starti++;
		}
	}

	if (pid == 0) {

		for (i = starti; i < numberOfNodes; i++) {

			for (j = startj; j < numberOfNodes; j++) {

				if (i == j) continue ;

				/*
				 * Generate all paths between current source and current 
				 * destination.
				 */

				pathList = pathsBuild(graph, i, j);

printf("%d paths were generated between nodes %d and %d:\n\n", listLength(pathList), i, j);
				/*
				 * Now comes the expensive part: we simulate all paths and store
				 * the resultant cost for each.
				 */

				path = listBegin(pathList);

				while(path) {

					arraySet(currentPaths, 0, prefixTreePath(path));
					currentCost = simulationSimulate(graph, currentPaths);
					prefixTreeSetSimulatedCost(path, currentCost);
					floatHeapAdd(heap, path, currentCost);
					path = listNext(pathList);
				}
				listFree(pathList);
				free(pathList);
				pathList = floatHeapListify(heap);
				
	//for (path = listBegin(pathList); path; path = listNext(pathList)) prefixTreePrint(path);
				sprintf(filename, "%s_%d_%d.sav", outputPrefix, i, j);
				prefixTreeSave(pathList, filename);
				
				for (path = listBegin(pathList); path; path = listNext(pathList)) prefixTreePrune(path);
				listFree(pathList);
				free(pathList);
			}
		}
	}
	else {

		while(numberOfProcs--) wait(NULL);
	}

	/*
	 * Free everything.
	 */

	arrayFree(currentPaths);
	free(currentPaths);
	listFreeWithData(dst);
	free(dst);
	listFreeWithData(src);
	free(src);
	graphFree(graph);
	free(graph);
	floatHeapFree(heap);
	free(heap);

	return(0);
}

