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
#include "heap.h"

#include "heuristics.h"
#include "dijkstra.h"

#include "memory.h"

typedef struct {

	unsigned long src;
	unsigned long dst;
	unsigned long flowId;
} t_edge;

typedef struct {

	unsigned long flowId;
	unsigned long hop;
	t_heap * links;
	t_weight cost;
} t_clique;

int main(int argc, char ** argv) {

	t_graph * graph;
	int * currentSrc, * currentDst;
	int i, j;
	t_array * currentPaths, * bestPaths, * path, * newPath;
	t_list * src, * dst;
	float currentCost, bestCost;
	int numberOfFlows;
	t_heap * cliques;
	t_clique * worstClique, * newClique, * pClique;
	unsigned long prev, current, next;
	int numberOfPaths, numberOfHops, newNumberOfHops;
	t_array * otherPath1, * otherPath2;
	t_array * reversePath;
	t_array * indexes;
	t_array * indexList;
	t_array * alternativePath;
	int k, l;
	unsigned long pathIndex, hopIndex;
	t_weight removeDueToCoding, worstLinkCost;
	unsigned long worstLinkHead, worstLinkTail, worstLinkFlow;
	int numberOfLoops = 1000;
	t_edge * newEdge, * worstEdge;
	int modified;

	if (argc != 2) {

		fprintf(stderr, "Use: %s <input>\n", argv[0]);
		exit(1);
	}

	graph = parserParse(argv[1], & src, & dst);

	/*
	 * Build the initial solution.
	 */

	numberOfFlows = listLength(src);

	currentPaths = arrayNew(numberOfFlows);
//printf("length = %d\n", numberOfFlows);
	for (i = 0; i < numberOfFlows; i++) {

		currentSrc = listCurrent(src);
		currentDst = listCurrent(dst);

		if (!(dijkstra(graph, * currentSrc, * currentDst, & path))) {

			fprintf(stderr, "Graph is not connected!\n");
			exit(1);
		}
//printf("Dijkstra's best path between %d and %d:\n", * currentSrc, * currentDst);
//printf("%lu", (unsigned long) arrayGet(path, 0));
//for (j = 1; j < arrayLength(path); j++)
//	printf(" -> %lu", (unsigned long) arrayGet(path, j));
//printf("\n");
//fflush(stdout);
		arraySet(currentPaths, i, path);

		listNext(src);
		listNext(dst);
	}

	listFreeWithData(src);
	free(src);
	listFreeWithData(dst);
	free(dst);

	/*
	 * Compute the cost of the initial solution.
	 */
	indexes = arrayNew(graphSize(graph));
	numberOfPaths = arrayLength(currentPaths);
	reversePath = arrayNew(graphSize(graph));
	bestPaths = arrayNew(numberOfPaths);

	bestCost = simulationSimulate(graph, currentPaths);
	for (i = 0; i < numberOfPaths; i++) {

		path = arrayGet(currentPaths, i);
		newPath = arrayNew(arrayLength(path));
		arraySet(bestPaths, i, newPath);

		for (j = 0; j < arrayLength(path); j++) {

			arraySet(newPath, j, arrayGet(path, j));
		}
	}

	while(numberOfLoops--) {

		cliques = heapNew();
		arrayClear(indexes);
/*		
printf("Current evaluated path list is:\n");
for (i = 0; i < numberOfPaths; i++) {
path = arrayGet(currentPaths, i);
printf("\t- %lu", (unsigned long) arrayGet(path, 0));
for (j = 1; j < arrayLength(path); j++)
	printf(" -> %lu", (unsigned long) arrayGet(path, j));
printf("\n");
}
fflush(stdout);
*/
		/*
		 * Build an index so that, given a node, we 
		 * know whether it belongs to one or more 
		 * paths and to which.
		 */

		for (i = 0; i < numberOfPaths; i++) {

			path = arrayGet(currentPaths, i);
			numberOfHops = arrayLength(path);

			for (j = 0; j < numberOfHops; j++) {

				/*
				 * Current node for current path.
				 */
				current = (unsigned long) arrayGet(path, j);

				/*
				 * Get its index list. Build one,
				 * if non-existent.
				 */
				indexList = arrayGet(indexes, current);
				if (indexList == NULL) {

					indexList = arrayNew(2 * numberOfPaths);
					arraySet(indexes, current, indexList);
					arrayClear(indexList);
				}

				/*
				 * Append both path index and hop index to
				 * the list. Notice we always sum 1 to the 
				 * value so that the value zero has the meaning 
				 * that the node doesn't belong to the path.
				 */
				arraySet(indexList, 2 * i, (void *) (i + 1));
				arraySet(indexList, 2 * i + 1, (void *) (j + 1));
			}
		}

		/*
		 * Now we proceed by assembling one clique for each node.
		 * We also keep track of the worst clique.
		 */

		for (i = 0; i < numberOfPaths; i++) {

			path = arrayGet(currentPaths, i);
			numberOfHops = arrayLength(path);

			for (j = 0; j < numberOfHops; j++) {
//printf("Assembling clique for hop %d of path %d\n", j, i);
				/*
				 * Current node for current path.
				 */
				current = (unsigned long) arrayGet(path, j);

				/*
				 * Get its index list. It must have
				 * been created before.
				 */
				indexList = arrayGet(indexes, current);

				/*
				 * Create and fill new clique info.
				 */
				MALLOC(newClique, sizeof(t_clique));
				newClique->flowId = i;
				newClique->hop = j;
				newClique->links = heapNew();
				newClique->cost = 0;

				/*
				 * Loop through every path this node 
				 * participates in.
				 */
				for (k = 0; k < numberOfPaths; k++) {

					pathIndex = (unsigned long) arrayGet(indexList, 2 * k);

					if (pathIndex == 0) continue ;

					hopIndex = (unsigned long) arrayGet(indexList, 2 * k + 1);
//printf("Node is in path %lu at hopIndex = %lu\n", pathIndex, hopIndex);
					otherPath1 = arrayGet(currentPaths, pathIndex - 1);

					prev = current;
					next = current;

					/*
					 * Is the current node the first in the path?
					 */
					if (hopIndex != 1) {

						/*
						 * No.
						 */

						prev = (unsigned long) arrayGet(otherPath1, hopIndex - 2);
					}

					/*
					 * Is the current node the last in the path?
					 */
					if (hopIndex != arrayLength(otherPath1)) {

						/*
						 * No.
						 */

						next = (unsigned long) arrayGet(otherPath1, hopIndex);
					}

					/*
					 * Before we add those (possibly) two links,
					 * we have to make sure there is no coding 
					 * possible. If necessary, go through all
					 * previous paths checking for opportunities.
					 */

					removeDueToCoding = 0;
					if (prev != current && next != current) {

						for (l = 0; l < numberOfPaths; l++) {

							pathIndex = (unsigned long) arrayGet(indexList, 2 * l);
							if (pathIndex == 0) continue ;
							hopIndex = (unsigned long) arrayGet(indexList, 2 * l + 1);

							otherPath2 = arrayGet(currentPaths, pathIndex - 1);
							if (otherPath1 == otherPath2) break ;

							/*
							 * If there is no previous hop in this
							 * path, there is no coding opportunity 
							 * here.
							 */
							if (hopIndex == 1) continue ;

							/*
							 * If the previous hop does not match the next hop we
							 * had before, there is no coding opportunity here.
							 */
							if (((unsigned long) arrayGet(otherPath2, hopIndex - 2)) != next) continue ;

							/*
							 * If there is no next hop in the path,
							 * there is no coding opportunity here.
							 */
							if (hopIndex == arrayLength(otherPath2)) continue ;

							/*
							 * If the next hop does not match the previous hop we
							 * had before, there is no coding opportunity here.
							 */
							if (((unsigned long) arrayGet(otherPath2, hopIndex)) != prev) continue ;

							/*
							 * If we got this far, there is a coding opportunity.
							 * Have to find out the link with lowest cost and 
							 * account for it.
							 * TODO: we may be underestimating the cost of the clique
							 * by not checking if this path was already selected for 
							 * coding opportunity.
							 */

							removeDueToCoding = graphGetCost(graph, current, prev);
							if (graphGetCost(graph, current, next) < removeDueToCoding) {

								removeDueToCoding = graphGetCost(graph, current, next);
								next = current;
							}
							else
								prev = current;

							break ;
						}
					}

					/*
					 * Sum the cost of the links and subtract any residues
					 * due to coding. Also, store the links in the clique.
					 */
					if (prev != current) {
//printf("Adding input link from node %lu\n", prev);
					
						MALLOC(newEdge, sizeof(t_edge));
						newEdge->src = prev;
						newEdge->dst = current;
						newEdge->flowId = k;
						heapAdd(newClique->links, (void *) newEdge, GRAPH_INFINITY - graphGetCost(graph, prev, current));
					}
					if (next != current) {

//printf("Adding output link for node %lu\n", next);

						MALLOC(newEdge, sizeof(t_edge));
						newEdge->src = current;
						newEdge->dst = next;
						newEdge->flowId = k;
						heapAdd(newClique->links, (void *) newEdge, GRAPH_INFINITY - graphGetCost(graph, current, next));
					}
//printf("current cost is " WEIGHT_FORMAT "\n", newClique->cost);
//printf("Removing from coding " WEIGHT_FORMAT "\n", removeDueToCoding);
//printf("prev = %lu , next = %lu, current = %lu\n", prev, next, current);
//printf("Values being summed are: " WEIGHT_FORMAT " + " WEIGHT_FORMAT " - " WEIGHT_FORMAT "\n", graphGetCost(graph, current, next), graphGetCost(graph, prev, current), removeDueToCoding);
					newClique->cost += (graphGetCost(graph, current, next) + graphGetCost(graph, prev, current) - removeDueToCoding);
//printf("New total cost is " WEIGHT_FORMAT "\n", newClique->cost);
				}

				/*
				 * Finally, we add the link from the next node in the path
				 * to the nextNext (if it exists).
				 */

				if (j < numberOfHops - 2) {

					MALLOC(newEdge, sizeof(t_edge));
					newEdge->src = (unsigned long) arrayGet(path, j + 1);
					newEdge->dst = (unsigned long) arrayGet(path, j + 2);
					newEdge->flowId = newClique->flowId;
					heapAdd(newClique->links, (void *) newEdge, GRAPH_INFINITY - graphGetCost(graph, newEdge->src, newEdge->dst));

					newClique->cost += graphGetCost(graph, newEdge->src, newEdge->dst);
									
				}

				heapAdd(cliques, newClique, GRAPH_INFINITY - newClique->cost);
			}
		}

		modified = 0;

		while(heapSize(cliques)) {

			worstClique = heapExtractMinimum(cliques, NULL);

			while(heapSize(worstClique->links)) {

				/*
				 * Now, we take the worst clique, find its worst link 
				 * and try and replace it with an alternative path.
				 */

				worstEdge = heapExtractMinimum(worstClique->links, NULL);
				worstLinkCost = graphGetCost(graph, worstEdge->src, worstEdge->dst);
				worstLinkHead = worstEdge->dst;
				worstLinkTail = worstEdge->src;
				worstLinkFlow = worstEdge->flowId;
				free(worstEdge);

//printf("----Link to be replaced is %lu -> %lu from flow %lu\n", worstLinkTail, worstLinkHead, worstLinkFlow);
				/*
				 * At this point, we have the worst link figured out. We'll disable this link
				 * in the graph so that we can find an alternative path between tail and head.
				 */
				graphDisableLink(graph, worstLinkTail, worstLinkHead);
				if (dijkstra(graph, worstLinkTail, worstLinkHead, & alternativePath) != GRAPH_INFINITY) {

					modified = 1;

					path = arrayGet(currentPaths, worstLinkFlow);
/*
printf("Alternative path is:\n\t-");
for (i = 0; i < arrayLength(alternativePath); i++)
	printf("%lu -> ", arrayGet(alternativePath, i));
printf("\n");
printf("Original path is:\n\t-");
for (i = 0; i < arrayLength(path); i++)
	printf("%lu -> ", arrayGet(path, i));
printf("\n");
*/
					/*
					 * We have to update the paths so that in the following
					 * loop we can proceed. We have to be careful because 
					 * the alternative subpath between tail and head might 
					 * have introduced loops (it might have used links) that 
					 * were already in the path. We have to process this.
					 * The way to do this is by representing the path as 
					 * an array of next hops for each node. That's what
					 * the "reversePath" variable is all about: the i'th 
					 * position of the array represents the next hop on 
					 * the path after that node. A zero represents that
					 * there is no next hop (hence, we always use id+1 in 
					 * this structure). We'll fill that structure
					 * and loops will be automatically overwritten.
					 */
					arrayClear(reversePath);

					/*
					 * First, the original path until the tail of the link.
					 */
					for (i = 1; i < arrayLength(path); i++) {

						if ((unsigned long) arrayGet(path, i - 1) == worstLinkTail) break ;

						arraySet(reversePath, (unsigned long) arrayGet(path, i - 1), 
							(void *) (((unsigned long) arrayGet(path, i)) + 1));
					}

					/*
					 * Then, the subpath we just found.
					 */
					for (j = 1; j < arrayLength(alternativePath); j++) {

						arraySet(reversePath, (unsigned long) arrayGet(alternativePath, j - 1), 
							(void *) (((unsigned long) arrayGet(alternativePath, j)) + 1));
					}

					/*
					 * Finally, the rest of the original path.
					 */
					for (i++; i < arrayLength(path); i++) {

						arraySet(reversePath, (unsigned long) arrayGet(path, i - 1), 
							(void *) (((unsigned long) arrayGet(path, i)) + 1));
					}
/*
printf("Reverse path:\n");
for (i = 0; i < graphSize(graph); i++)
	printf("%lu -- ", arrayGet(reversePath, i));
printf("\n");
fflush(stdout);
*/
					/*
					 * Follow the formed path to find out the new number of hops.
					 */
					newNumberOfHops = 0;
					current = (unsigned long) arrayGet(path, 0);
					while((current = (unsigned long) arrayGet(reversePath, current))) {

						current--;
						newNumberOfHops++;
						if (current == (unsigned long) arrayGet(path, arrayLength(path) - 1)) break ;
					}

					/*
					 * Aloc a new path.
					 */
					newPath = arrayNew(newNumberOfHops + 1);

//printf("New path has %d hops:\n", newNumberOfHops + 1);
//fflush(stdout);
					/*
					 * Fill the new path.
					 */
					current = (unsigned long) arrayGet(path, 0);
//printf("Adding node %lu to output path.\n", current);
					arraySet(newPath, 0, (void *) current);
					j = 1;
					while((current = (unsigned long) arrayGet(reversePath, current))) {

						current--;
//printf("Adding node %lu to output path.\n", current);
						arraySet(newPath, j++, (void *) current);
						if (current == (unsigned long) arrayGet(path, arrayLength(path) - 1)) break ;
					}

					/*
					 * Delete what we had before and store the new array for the path.
					 */
					arrayFree(path);
					free(path);
					arraySet(currentPaths, worstLinkFlow, newPath);

					/*
					 * Run the simulation.
					 */
					currentCost = simulationSimulate(graph, currentPaths);
//printf("@@@@@@@@@@@@@@@@@currentCost = %.2f vs %.2f\n", currentCost, bestCost);
					if (currentCost < bestCost) {

						for (i = 0; i < numberOfPaths; i++) {

							path = arrayGet(bestPaths, i);
							arrayFree(path);
							free(path);

							path = arrayGet(currentPaths, i);
							newPath = arrayNew(arrayLength(path));
							arraySet(bestPaths, i, newPath);

							for (j = 0; j < arrayLength(path); j++) {

								arraySet(newPath, j, arrayGet(path, j));
							}
						}

						bestCost = currentCost;
					}

					arrayFree(alternativePath);
					free(alternativePath);

					break ;
				}
			}

			heapFreeWithData(worstClique->links);
			free(worstClique->links);
			free(worstClique);

			if (modified) break ;
		}

		while ((pClique = heapExtractMinimum(cliques, NULL))) {

			heapFreeWithData(pClique->links);
			free(pClique->links);
			free(pClique);
		}

		heapFree(cliques);
		free(cliques);

		for (i = 0; i < graphSize(graph); i++) {

	
			indexList = arrayGet(indexes, i);
			if (indexList) {
				
				arrayFree(indexList);
				free(indexList);
			}
		}

		if (modified == 0) {

//printf("Bailing on no changes possible for any clique.\n");
			break ;
		}
	}
	printf("Best path set has cost %.2f and is:\n", bestCost / GRAPH_MULTIPLIER);
	for (i = 0; i < arrayLength(bestPaths); i++) {

		for (j = 0; j < arrayLength(arrayGet(bestPaths, i)); j++) {
			
			printf("%lu ", (unsigned long) arrayGet(arrayGet(bestPaths, i), j));
		}
		printf("\n");
	}

	arrayFree(reversePath);
	free(reversePath);

	for (i = 0; i < arrayLength(bestPaths); i++) {
		
		arrayFree(arrayGet(bestPaths, i));
		free(arrayGet(bestPaths, i));
	}
	arrayFree(bestPaths);
	free(bestPaths);

	for (i = 0; i < arrayLength(currentPaths); i++) {
		
		arrayFree(arrayGet(currentPaths, i));
		free(arrayGet(currentPaths, i));
	}
	arrayFree(currentPaths);
	free(currentPaths);

	arrayFree(indexes);
	free(indexes);

	graphFree(graph);
	free(graph);

	return(0);
}

