#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <time.h>
#include <sys/resource.h>

#define _ISOC99_SOURCE
#include <math.h>

#include "graph.h"
#include "parserOnlyPaths.h"
#include "prefixTree.h"
#include "list.h"
#include "array.h"
#include "simulationh2.h"
#include "memory.h"

#include "heuristics.h"
#include "dijkstra.h"

//#include "evaluateSimulationAux.h"
//#include "evaluateSimulationAuxB.h"

int main(int argc, char ** argv) {

	int * currentNode;
	int i, j;
	t_graph * graph;
	t_list * parsedPathList;
	t_array * currentPaths;
	t_return * r;
	t_list * parsedPath;
	t_array * newPath;
	struct rusage usage;
	t_list * flt;
	t_array * flowTimes;
	int * currentFlt;

	t_list * ftx;
	t_array * txDurations;
	int * currentTxTime;

	if (argc != 2) {

		fprintf(stderr, "Use: %s <input>\n", argv[0]);
		exit(1);
	}

	MALLOC(r, sizeof(t_return));

	graph = parserParse(argv[1], & parsedPathList, & flt, & ftx);

	currentPaths = arrayNew(listLength(parsedPathList));

	flowTimes = arrayNew(listLength(flt));
	listBegin(flt);

	txDurations = arrayNew(listLength(ftx));
	listBegin(ftx);

	i = 0;
	for (parsedPath = listBegin(parsedPathList); parsedPath; parsedPath = listNext(parsedPathList)) {

		currentFlt = listCurrent(flt);
		arraySet(flowTimes, i, (void *) ((unsigned long) (* currentFlt))); // atribui o intevalo do fluxo

		currentTxTime = listCurrent(ftx);
		//printf(">>>> %.9f", currentTxTime);
		arraySet(txDurations, i, (void *) ((unsigned long) (* currentTxTime))); // atribui o tempo de transmissao de um frame

		j = 0;
		newPath = arrayNew(listLength(parsedPath));
		for (currentNode = listBegin(parsedPath); currentNode; currentNode = listNext(parsedPath)) {

			arraySet(newPath, j++, (void *) ((unsigned long) (* currentNode)));
		}

		listFreeWithData(parsedPath);
		arraySet(currentPaths, i++, newPath);
		listNext(flt);
		listNext(ftx);
	}
	listFreeWithData(parsedPathList);
	free(parsedPathList);
		
#ifdef OLD

	printf("Trying path set:\n");
	for (i = 0; i < arrayLength(currentPaths); i++) {

		for (j = 0; j < arrayLength(arrayGet(currentPaths, i)); j++) {
			
			printf("%lu ", (unsigned long) arrayGet(arrayGet(currentPaths, i), j));
		}
		printf("\n");
	}
fflush(stdout);
#endif


	printf("Starting Simulation...\n");

	r = simulationSimulate(graph, currentPaths, flowTimes, txDurations);

	getrusage(RUSAGE_SELF, & usage);
	printf("Done in %.6f\n", usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1.0e6);
	for (j = 0; j < i; j++) {
		printf("Flow %d cost: %.2f\n", j, r->meanIntervalPerFlow[j]);
	}
	//printf("%.2f\n", currentCost / GRAPH_MULTIPLIER);
	return(0);
}

