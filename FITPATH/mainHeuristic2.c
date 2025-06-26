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
#include "set.h"

#include "heuristics.h"
#include "dijkstra.h"

#include "memory.h"

typedef struct {

	unsigned long flowId;
	unsigned long hop;
	t_list * input;
	t_list * inputFlow;
	t_list * output;
	t_list * outputFlow;
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
	t_list * cliques;
	t_clique * worstClique, * newClique, * pClique;
	unsigned long prev, current, next, nextNext;
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
	int numberOfLoops = 10;


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

		cliques = listNew();
		arrayClear(indexes);
//printf("Current evaluated path list is:\n");
//for (i = 0; i < numberOfPaths; i++) {
//path = arrayGet(currentPaths, i);
//printf("\t- %lu", (unsigned long) arrayGet(path, 0));
//for (j = 1; j < arrayLength(path); j++)
//	printf(" -> %lu", (unsigned long) arrayGet(path, j));
//printf("\n");
//}
//fflush(stdout);

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

		worstClique = NULL;
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
				newClique->input = listNew();
				newClique->output = listNew();
				newClique->inputFlow = listNew();
				newClique->outputFlow = listNew();
				newClique->cost = 0;

				listAdd(cliques, newClique);

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
					 * Notice also that, when we store a node Id, we again
					 * do it with nodeId + 1.
					 */
					if (prev != current) {
//printf("Adding input link from node %lu\n", prev);
						listAdd(newClique->input, (void *) (prev + 1));
						listAdd(newClique->inputFlow, (void *) (k + 1));
					}
					if (next != current) {

//printf("Adding output link for node %lu\n", next);
						listAdd(newClique->output, (void *) (next + 1));
						listAdd(newClique->outputFlow, (void *) (k + 1));
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

					newClique->cost += graphGetCost(graph, (unsigned long) arrayGet(path, j + 1), 
									(unsigned long) arrayGet(path, j + 2));
				}

				/*
				 * Check if this is the worst clique so far.
				 */
				if (worstClique) {

					if (worstClique->cost < newClique->cost) worstClique = newClique;
				}
				else 
					worstClique = newClique;
			}
		}

		/*
		 * Here, we begin the maintenance phase.
		 * We take the worst clique, find its worst link 
		 * and try and replace it with an alternative path.
		 */

		worstLinkCost = 0;
		worstLinkHead = 0;
		worstLinkTail = 0;
		worstLinkFlow = 0;

		path = arrayGet(currentPaths, worstClique->flowId);

		current = (unsigned long) arrayGet(path, worstClique->hop);

//printf("-------Worst clique is with flow %lu and has hop %lu (current = %lu)\n", worstClique->flowId, worstClique->hop, current);
		/*
		 * We start by evaluating the input links.
		 */

		listBegin(worstClique->inputFlow);
		for (prev = (unsigned long) listBegin(worstClique->input); prev; prev = (unsigned long) listNext(worstClique->input)) {

			if (worstLinkCost < graphGetCost(graph, prev - 1, current)) {

				worstLinkCost = graphGetCost(graph, prev - 1, current);
				worstLinkHead = current;
				worstLinkTail = prev - 1;
				worstLinkFlow = ((unsigned long) listCurrent(worstClique->inputFlow)) - 1;
			}

			listNext(worstClique->inputFlow);
		}

		/*
		 * We do the same for the output links.
		 */

		listBegin(worstClique->outputFlow);
		for (next = (unsigned long) listBegin(worstClique->output); next; next = (unsigned long) listNext(worstClique->output)) {

			if (worstLinkCost < graphGetCost(graph, current, next - 1)) {

				worstLinkCost = graphGetCost(graph, next - 1, current);
				worstLinkHead = next - 1;
				worstLinkTail = current;
				worstLinkFlow = ((unsigned long) listCurrent(worstClique->outputFlow)) - 1;
			}

			listNext(worstClique->outputFlow);
		}

		/*
		 * Now, there may be one implicit link, which is the next hop of the path
		 * originaly contained by the click. We first have to check if the link
		 * actually exists. If it does, we check if it is worse than what we have
		 * so far.
		 */
		if (arrayLength(arrayGet(currentPaths, worstClique->flowId)) > worstClique->hop + 2) {

			next = (unsigned long) arrayGet(arrayGet(currentPaths, worstClique->flowId), worstClique->hop + 1);
			nextNext = (unsigned long) arrayGet(arrayGet(currentPaths, worstClique->flowId), worstClique->hop + 2);

			if (graphGetCost(graph, next, nextNext) > worstLinkCost) {

				/*
				 * So this link is the worse. In this case, we have to 
				 * discover who is the neighbor.
				 */
				worstLinkCost = graphGetCost(graph, next, nextNext);
				worstLinkHead = nextNext;
				worstLinkTail = next;
				worstLinkFlow = worstClique->flowId;
			}
		}
//printf("Link to be replaced is %lu -> %lu from flow %lu\n", worstLinkTail, worstLinkHead, worstLinkFlow);
		/*
		 * At this point, we have the worst link figured out. We'll disable this link
		 * in the graph so that we can find an alternative path between tail and head.
		 */
		graphDisableLink(graph, worstLinkTail, worstLinkHead);
		if (dijkstra(graph, worstLinkTail, worstLinkHead, & alternativePath) != GRAPH_INFINITY) {

			path = arrayGet(currentPaths, worstLinkFlow);

//printf("Alternative path is:\n\t-");
//for (i = 0; i < arrayLength(alternativePath); i++)
//	printf("%lu -> ", arrayGet(alternativePath, i));
//printf("\n");
//printf("Original path is:\n\t-");
//for (i = 0; i < arrayLength(path); i++)
//	printf("%lu -> ", arrayGet(path, i));
//printf("\n");
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



//printf("Reverse path:\n");
//for (i = 0; i < graphSize(graph); i++)
//	printf("%lu -- ", arrayGet(reversePath, i));
//printf("\n");
//fflush(stdout);

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
//printf("currentCost = %.2f vs %.2f\n", currentCost, bestCost);
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
		}
		else {

			/*
			 * We could not find an alternative path. TODO: figure it out.
			 * For now, in order to test what we have so far, we simply 
			 * consider this to be a stop condition.
			 */

//printf("Bailing on alternative path not found.\n");

			for (pClique = listBegin(cliques); pClique; pClique = listNext(cliques)) {

				listFree(pClique->input);
				listFree(pClique->inputFlow);
				listFree(pClique->output);
				listFree(pClique->outputFlow);
				free(pClique->input);
				free(pClique->inputFlow);
				free(pClique->output);
				free(pClique->outputFlow);
			}
		
			listFreeWithData(cliques);
			free(cliques);

			for (i = 0; i < graphSize(graph); i++) {

				
				indexList = arrayGet(indexes, i);
				if (indexList) {
					
					arrayFree(indexList);
					free(indexList);
				}
			}

			break ;
		}

		for (pClique = listBegin(cliques); pClique; pClique = listNext(cliques)) {

			listFree(pClique->input);
			listFree(pClique->inputFlow);
			listFree(pClique->output);
			listFree(pClique->outputFlow);
			free(pClique->input);
			free(pClique->inputFlow);
			free(pClique->output);
			free(pClique->outputFlow);
		}
			
		listFreeWithData(cliques);
		free(cliques);

		for (i = 0; i < graphSize(graph); i++) {

			
			indexList = arrayGet(indexes, i);
			if (indexList) {
				
				arrayFree(indexList);
				free(indexList);
			}
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

