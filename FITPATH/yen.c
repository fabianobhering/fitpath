#include <stdio.h>

#include "graph.h"
#include "list.h"
#include "array.h"
#include "prefixTree.h"
#include "heap.h"
#include "dijkstra.h"
#include "memory.h"

t_list * yen(t_graph * graph, int source, int destination, int numberOfPaths) {

	t_list * output;
	t_list * nextHops;
	t_array * path;
	t_heap * candidates;
	t_array * candidate;
	t_prefixTreeNode * root, * lastInsertedPath, * prefix, * sufix, * p;
	t_weight cost;

	output = listNew();
	candidates = heapNew();
	root = prefixTreeNew(source);

	/*
	 * First path is easy: run dijkstra.
	 */
	cost = dijkstra(graph, source, destination, & path);
	if (cost == GRAPH_INFINITY) {

//		fprintf("Not paths for pair %d, %d\n", source, destination);
		heapFree(candidates);
		free(candidates);

		return(output);
	}
//printf("--Adding path with nominal cost of %d\n", cost);
	lastInsertedPath = prefixTreeInsert(root, path, graph);
	listAdd(output, lastInsertedPath);
	arrayFree(path);
	free(path);

	/*
	 * Repeat for the remaining paths.
	 */
	while(--numberOfPaths) {

		/*
		 * We'll generate a bunch of candidates. Basically,
		 * we're are gonna evaluate every prefix of the last
		 * used path (of sizes from 1 to (nHops-1)) and we'll 
		 * try to find the best possible sufix different from 
		 * the sufix of the last path.
		 */
		prefix = prefixTreeGetPrefix(lastInsertedPath);
		while(prefix) {

			/*
			 * Remove the link for the next hop in all paths
			 * that share this common prefix with the last
			 * path.
			 */
			nextHops = prefixTreeGetSufixes(prefix);
			for (sufix = listBegin(nextHops); sufix; sufix = listNext(nextHops)) {

				graphDisableLink(graph, prefixTreeGetNode(prefix), prefixTreeGetNode(sufix));
			}

			/*
			 * Remove all nodes from the prefix so that we
			 * don't choose them (to avoid cycles).
			 */
			p = prefixTreeGetPrefix(prefix);
			while(p/* && p != root*/) {

				graphDisableNode(graph, prefixTreeGetNode(p));
				p = prefixTreeGetPrefix(p);
			}

			/*
			 * Run dijkstra on the resulting graph to 
			 * obtain a new candidate. Notice we only 
			 * need to find a sufix.
			 */
			cost = dijkstra(graph, prefixTreeGetNode(prefix), destination, & path);
			if (cost < GRAPH_INFINITY) {

				candidate = arrayNew(2);
				arraySet(candidate, 0, prefix);
				arraySet(candidate, 1, path);
				heapAdd(candidates, candidate, cost + prefixTreeGetCost(prefix));
			}

			graphReenableAll(graph);
			prefix = prefixTreeGetPrefix(prefix);
		}

		while(1) {

			candidate = heapExtractMinimum(candidates, & cost);
			if (candidate == NULL) {

				while((candidate = heapExtractMinimum(candidates, & cost))) {

					arrayFree(path);
					arrayFree(candidate);
					free(path);
					free(candidate);
				}
				
				heapFree(candidates);
				free(candidates);

//				fprintf("Not enought paths for pair %d, %d\n", source, destination);
				return(output);
			}

//printf("Adding path with nominal cost of %d\n", cost);
			prefix = arrayGet(candidate, 0);
			path = arrayGet(candidate, 1);
			lastInsertedPath = prefixTreeInsert(prefix, path, graph);
			if (lastInsertedPath == NULL) {

				/*
				 * prefixTreeInsert only returns NULL when the
				 * path we are trying to insert already exists.
				 * Therefore, this is not a usefull path for us.
				 * We have to keep digging the candidate set until we
				 * find one. We also free everything before continuing.
				 */
				arrayFree(path);
				arrayFree(candidate);
				free(path);
				free(candidate);

				continue ;
			}

			listAdd(output, lastInsertedPath);

			arrayFree(path);
			arrayFree(candidate);
			free(path);
			free(candidate);

			break ;
		}
	}

	while((candidate = heapExtractMinimum(candidates, & cost))) {

		prefix = arrayGet(candidate, 0);
		path = arrayGet(candidate, 1);

		arrayFree(path);
		arrayFree(candidate);
		free(path);
		free(candidate);
	}
	
	heapFree(candidates);
	free(candidates);

	return(output);
}


