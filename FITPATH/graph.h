#ifndef __GRAPH_H__
#define __GRAPH_H__

#define _ISOC99_SOURCE

#include <math.h>

#include "list.h"
#include "set.h"

#include <stdint.h>

#ifdef USE_INT_WEIGHT
	typedef uint64_t t_weight;
	#define GRAPH_INFINITY	UINT64_MAX
        #ifdef __LP64__
                #define WEIGHT_FORMAT   "%lu"
        #else
                #define WEIGHT_FORMAT   "%llu"
        #endif
	#define GRAPH_MULTIPLIER 10000
#else
	typedef float t_weight;
	#define GRAPH_INFINITY	INFINITY
	#define WEIGHT_FORMAT	"%.2f"
	#define GRAPH_MULTIPLIER 1.0
#endif

typedef struct {

	int numberOfNodes;
	t_list ** neighborhood;
	t_set ** neighborDisabled;
	t_weight ** adj;
} t_graph;

t_graph * graphNew(int numberOfNodes);
void graphAddLink(t_graph * graph, int src, int dst, float cost);
t_list * graphGetNeighbors(t_graph * graph, int node);
int graphSize(t_graph * graph);
t_weight graphGetCost(t_graph * graph, int src, int dst);
void graphFree(t_graph * graph);
void graphPrint(t_graph * graph);
void graphDisableLink(t_graph * graph, int src, int dst);
void graphDisableNode(t_graph * graph, int node);
void graphReenableAll(t_graph * graph);
int graphIsDisabled(t_graph * graph, int src, int dst);

#endif

