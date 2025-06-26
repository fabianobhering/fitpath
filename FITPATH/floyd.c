#include "floyd.h"
#include "memory.h"

#include <string.h>

t_floyd_solution floydSolve(t_graph * graph) {

	t_floyd_solution solution;
	int i, j, k, n;

	n = graphSize(graph);
//graphPrint(graph);
	MALLOC(solution, n * sizeof(t_flyod_entry *));
	for (i = 0; i < n; i++) {

		MALLOC(solution[i], n * sizeof(t_flyod_entry));
		memset(solution[i], 0, n * sizeof(t_flyod_entry));
	}

	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++) {

			if (j == i)
				solution[i][j].cost = GRAPH_INFINITY;
			else
				solution[i][j].cost = graphGetCost(graph, i, j);
			solution[i][j].next = j;
			solution[i][j].hops = 1;
		}
	}

	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++) {
			for (k = 0; k < n; k++) {

				if (j == k) continue ;
				if (solution[j][i].cost == GRAPH_INFINITY) continue ;
				if (solution[i][k].cost == GRAPH_INFINITY) continue ;
//printf("%d -> %d por %d: " WEIGHT_FORMAT " + " WEIGHT_FORMAT " vs. " WEIGHT_FORMAT "\n",
//	j, k, i, solution[j][i].cost, solution[i][k].cost, solution[j][k].cost);
				if (solution[j][i].cost + solution[i][k].cost < solution[j][k].cost) {
//printf("Solucao melhorada.\n");
					solution[j][k].cost = solution[j][i].cost + solution[i][k].cost;
					solution[j][k].next = solution[j][i].next;
					solution[j][k].hops = solution[j][i].hops + solution[i][k].hops;
				}
			}
		}
	}
/*
for (i = 0; i < n; i++) {
	for (j = 0; j < n; j++) {

		if (solution[i][j].cost == GRAPH_INFINITY)			
printf("inf ");
else
printf(WEIGHT_FORMAT " ", solution[i][j].cost);
		}
printf("\n");
	}
*/
	return(solution);
}

int floydPathLength(t_floyd_solution solution, int src, int dst) {

	return(solution[src][dst].hops);
}

t_weight floydGetCost(t_floyd_solution solution, int src, int dst) {

	return(solution[src][dst].cost);
}
int floydPathHop(t_floyd_solution solution, int src, int dst, int idx) {

	int i, next;

	next = src;
	for (i = 0; i < idx; i++) {

		next = solution[next][dst].next;
	}

	return(next);
}

void floydFree(t_floyd_solution solution, t_graph * graph) {

	int i, n;

	n = graphSize(graph);
	for (i = 0; i < n; i++) free(solution[i]);

	free(solution);
}

t_array * floydGetPath(t_floyd_solution solution, int src, int dst) {

	t_array * output;
	int i, next;
	int nodes;

	nodes = solution[src][dst].hops + 1;
	output = arrayNew(nodes);

	next = src;
	for (i = 0; i < nodes; i++) {

		arraySet(output, i, (void *) (long) next);
		next = solution[next][dst].next;
	}

	return(output);
}
