#include "graph.h"
#include "memory.h"

t_graph * graphNew(int numberOfNodes) {

	t_graph * graph;
	int i, j;

	MALLOC(graph, sizeof(t_graph));
	graph->numberOfNodes = numberOfNodes;

	MALLOC(graph->neighborhood, numberOfNodes * sizeof(t_list *));
	for (i = 0; i < numberOfNodes; i++) graph->neighborhood[i] = listNew();

	MALLOC(graph->neighborDisabled, numberOfNodes * sizeof(t_set *));
	for (i = 0; i < numberOfNodes; i++) graph->neighborDisabled[i] = setNew();

	MALLOC(graph->adj, numberOfNodes * sizeof(t_weight *));
	for (i = 0; i < numberOfNodes; i++) {
		
		MALLOC(graph->adj[i], numberOfNodes * sizeof(t_weight));
		for (j = 0; j < numberOfNodes; j++) {
		
			if (i == j) graph->adj[i][j] = 0;
			else graph->adj[i][j] = GRAPH_INFINITY;
		}
	}

	return(graph);
}

void graphAddLink(t_graph * graph, int src, int dst, float cost) {

	int * node;

	if (graph->adj[src][dst] == GRAPH_INFINITY) {

		MALLOC(node, sizeof(int));
		* node = dst;
		graph->adj[src][dst] = (t_weight) GRAPH_MULTIPLIER * cost;
		listAdd(graph->neighborhood[src], node);
	}
	else if (src == dst && graph->adj[src][dst] == 0) {

		MALLOC(node, sizeof(int));
		* node = dst;
		graph->adj[src][dst] = (t_weight) GRAPH_MULTIPLIER * cost;
		listAdd(graph->neighborhood[src], node);
	}
}

t_list * graphGetNeighbors(t_graph * graph, int node) {

	return(graph->neighborhood[node]);
}

int graphSize(t_graph * graph) {

	return(graph->numberOfNodes);
}

t_weight graphGetCost(t_graph * graph, int src, int dst) {

	return(graph->adj[src][dst]);
}

void graphFree(t_graph * graph) {
	
	int i;

	for (i = 0; i < graph->numberOfNodes; i++) {

		listFreeWithData(graph->neighborhood[i]);
		free(graph->neighborhood[i]);
		setFree(graph->neighborDisabled[i]);
		free(graph->neighborDisabled[i]);
		free(graph->adj[i]);
		
	}

	free(graph->neighborhood);
	free(graph->neighborDisabled);
	free(graph->adj);
}

void graphDisableLink(t_graph * graph, int src, int dst) {

	setAdd(graph->neighborDisabled[src], dst);
}

void graphDisableNode(t_graph * graph, int node) {

	int i;

	for (i = 0; i < graph->numberOfNodes; i++) {

		if (i == node) continue ;
		if (graph->adj[i][node] < GRAPH_INFINITY) 
			setAdd(graph->neighborDisabled[i], node);
	}
}

void graphReenableAll(t_graph * graph) {

	int i;

	for (i = 0; i < graph->numberOfNodes; i++) {
	
		setClear(graph->neighborDisabled[i]);
	}
}

int graphIsDisabled(t_graph * graph, int src, int dst) {

	return(setIsElementOf(graph->neighborDisabled[src], dst));
}

void graphPrint(t_graph * graph) {

	int i, j;
	int * neighbor;

	printf("Adjacence matrix:\n\n");
	for (i = 0; i < graph->numberOfNodes; i++) {
		for (j = 0; j < graph->numberOfNodes; j++) {

			if (graph->adj[i][j] == GRAPH_INFINITY)
				printf("inf ");
			else
				printf(WEIGHT_FORMAT " ", graph->adj[i][j]);
		}
		printf("\n");
	}
	printf("\n");

	printf("Neighborhood:\n\n");
	for (i = 0; i < graph->numberOfNodes; i++) {
		printf("%d : ", i);
		for (neighbor = listBegin(graph->neighborhood[i]); neighbor; neighbor = listNext(graph->neighborhood[i])) {

			printf(" %d,", * neighbor);
		}
		printf("\n");
	}
}


