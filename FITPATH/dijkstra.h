#ifndef __DIJKSTRA_H__
#define __DIJKSTRA_H__

#include "graph.h"
#include "array.h"

t_weight dijkstra(t_graph * graph, long source, long destination, t_array ** output);

#endif

