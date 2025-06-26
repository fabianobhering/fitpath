#ifndef __FLOYD_H__
#define __FLOYD_H__ 

#include "graph.h"
#include "array.h"


typedef struct {

	t_weight cost;
	int next;
	int hops;
} t_flyod_entry;

typedef t_flyod_entry ** t_floyd_solution;

t_floyd_solution floydSolve(t_graph * graph);
int floydPathLength(t_floyd_solution solution, int src, int dst);
int floydPathHop(t_floyd_solution solution, int src, int dst, int idx);
void floydFree(t_floyd_solution solution, t_graph * graph);
t_weight floydGetCost(t_floyd_solution solution, int src, int dst);
t_array * floydGetPath(t_floyd_solution solution, int src, int dst);

#endif 

