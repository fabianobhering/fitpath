#include "paths.h"
#include "stack.h"
#include "graph.h"
#include "list.h"
#include "prefixTree.h"
#include "memory.h"

t_list * pathsBuild(t_graph * graph, int src, int dst) {

	t_stack * stack;
	t_list * listOfNeighbors;
	t_list * outputPaths;
	t_prefixTreeNode * prefix, * newPrefix;
	int currentNode, * neighbor;

	outputPaths = listNew();
	stack = stackNew();
	prefix = prefixTreeNew(src);

	listOfNeighbors = graphGetNeighbors(graph, src);
	for (neighbor = listBegin(listOfNeighbors); neighbor; neighbor = listNext(listOfNeighbors)) {

		newPrefix = prefixTreeAppend(prefix, * neighbor, graphGetCost(graph, src, * neighbor));

		if (* neighbor == dst) {

			listAdd(outputPaths, newPrefix);
			continue ;
		}

		stackPush(stack, newPrefix);
	}
	
	while(!stackIsEmpty(stack)) {

		prefix = stackPop(stack);
		currentNode = prefixTreeGetNode(prefix);
		listOfNeighbors = graphGetNeighbors(graph, currentNode);
		for (neighbor = listBegin(listOfNeighbors); neighbor; neighbor = listNext(listOfNeighbors)) {

			if (prefixTreeNodeExists(prefix, * neighbor)) continue ;

			newPrefix = prefixTreeAppend(prefix, * neighbor, graphGetCost(graph, currentNode, * neighbor));

			if (* neighbor == dst) {

				listAdd(outputPaths, newPrefix);
				continue ;
			}

			stackPush(stack, newPrefix);
		}

		if (prefixTreeGetRefCount(prefix) == 0) prefixTreePrune(prefix);
	}

	stackFree(stack);
	free(stack);

	return(outputPaths);
}

