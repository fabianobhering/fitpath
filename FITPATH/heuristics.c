#include "array.h"
#include "graph.h"
#include "set.h"

#include "dijkstra.h"

#include <stdio.h>
#include <stdlib.h>

t_array * outputWeights, * inputWeights, * indexes, * candidates;
t_set * alreadyVisited;

void heuristicInit(int numberOfNodes, int numberOfPaths) {

	int i;

	outputWeights = arrayNew(numberOfNodes);
	inputWeights = arrayNew(numberOfNodes);
	candidates = arrayNew(numberOfNodes);
	indexes = arrayNew(numberOfNodes);
	alreadyVisited = setNew();

	for (i = 0; i < numberOfNodes; i++) {

		arraySet(indexes, i, arrayNew(numberOfPaths + 1));
	}
}

void heuristicFinalize() {

	int i;

	arrayFree(outputWeights);
	arrayFree(inputWeights);
	free(outputWeights);
	free(inputWeights);

	for (i = 0; i < arrayLength(indexes); i++) {
		
		arrayFree((void *) arrayGet(indexes, i));
		free(arrayGet(indexes, i));
	}
	arrayFree(indexes);
	free(indexes);
	setFree(alreadyVisited);
}

float heuristicMultiplePathNewlowerBound(t_graph * graph, t_array * paths, float * costs, float breakAt) {

	long i, j, k;
	int numberOfPaths, numberOfNodes, numberOfCandidates;
	t_array * path, * path2;
	unsigned long prev, prevPrev, current, next, nextNext;
	unsigned long indexInOtherPath, indexInOtherPath2;
	unsigned long currentClique, worstClique;
	float worstEstimate, currentEstimate;
	float delayEstimate; // TODO: generalize the type.

	arrayClear(outputWeights);
	arrayClear(inputWeights);
	arrayClear(candidates);

	numberOfPaths = arrayLength(paths);
	numberOfCandidates = 0;

	if (breakAt < INFINITY) breakAt *= numberOfPaths;

	for (i = 0; i < numberOfPaths; i++) {
		
		path = arrayGet(paths, i);
		numberOfNodes = arrayLength(path);
		for (j = 0; j < numberOfNodes; j++) {

			current = (unsigned long) arrayGet(path, j);

			if (arrayGet(outputWeights, current) == 0 && arrayGet(inputWeights, current) == 0) {

				arrayClear(arrayGet(indexes, current));
			}

			arraySet(arrayGet(indexes, current), i, (void *) (j + 1));

			arrayInc(arrayGet(indexes, current), numberOfPaths);
			if (((unsigned long) arrayGet(arrayGet(indexes, current), numberOfPaths)) == 2)
				arraySet(candidates, numberOfCandidates++, (void *) current);

			if (j < numberOfNodes - 1) {

				next = (unsigned long) arrayGet(path, j + 1);
				arrayIncBy(outputWeights, current, graphGetCost(graph, current, next));
			}
			if (j > 0) {

				prev = (unsigned long) arrayGet(path, j - 1);
				arrayIncBy(inputWeights, current, graphGetCost(graph, prev, current));
			}
		}
	}

	worstEstimate = INFINITY;
	for (i = 0; i < numberOfCandidates; i++) {
		
		current = (unsigned long) arrayGet(candidates, i);
		delayEstimate = (unsigned long) arrayGet(inputWeights, current) + (unsigned long) arrayGet(outputWeights, current);
		currentEstimate = 0;
//printf("Current == %lu\n", current);
//		setClear(alreadyVisited);
//printf("Zeroing estimate: delayEstimate = %.2f\n", delayEstimate);
		worstClique = 0;
		for (j = 0; j < numberOfPaths; j++) {

			currentClique = 0;

			indexInOtherPath = (unsigned long) arrayGet(arrayGet(indexes, current), j);
			if (indexInOtherPath == 0) {

				currentEstimate += 1.0/costs[j];
				continue ;
			}

			path = arrayGet(paths, j);

			if (indexInOtherPath > 1) {

				prev = (unsigned long) arrayGet(path, indexInOtherPath - 2);
			}
			else {

				prev = (unsigned int) -1;
			}

			if (indexInOtherPath < arrayLength(path)) {

				next = (unsigned long) arrayGet(path, indexInOtherPath);

				for (k = 0; k < numberOfPaths; k++) {
//printf("Evaluating coding and next link in path %d\n", k);
					indexInOtherPath2 = (unsigned long) arrayGet(arrayGet(indexes, next), k);

					if (indexInOtherPath2 == 0) {

						continue ;
					}

					path2 = arrayGet(paths, k);

					if (indexInOtherPath2 < arrayLength(path2)) {

						nextNext = (unsigned long) arrayGet(path2, indexInOtherPath2);
						if (nextNext != current) {

//							currentClique += graphGetCost(graph, next, nextNext);
//printf("Adding cost of link %lu -> %lu to estimate: delayEstimate = %.2f\n", next, nextNext, delayEstimate);
						}
						else {

							if (indexInOtherPath2 < arrayLength(path2) - 1) {
								prevPrev = (unsigned long) arrayGet(path2, indexInOtherPath2 + 1);
								if (prev == prevPrev) {

//printf("Found coding! What is the decrease: %lu or %lu?\n", graphGetCost(graph, current, prev), graphGetCost(graph, current, next));
									if (graphGetCost(graph, current, prev) < graphGetCost(graph, current, next))
										delayEstimate -= graphGetCost(graph, current, prev);
									else
										delayEstimate -= graphGetCost(graph, current, next);
								}
							}
						}
					}
				}
			}

			if (currentClique > worstClique) worstClique = currentClique;
		}

//printf("Throughput for independent flows is %.2f\n", currentEstimate);
//printf("delayEstimate for %lu flows is %.2f\n", ((unsigned long) arrayGet(arrayGet(indexes, current), numberOfPaths)), delayEstimate);
//printf("Worst clique found is %lu\n", worstClique);

		currentEstimate += ((float) ((unsigned long) arrayGet(arrayGet(indexes, current), numberOfPaths))) / (delayEstimate + worstClique);
		
//printf("New estimate = %e\n", currentEstimate);

		if (currentEstimate < worstEstimate) {

			worstEstimate = currentEstimate;
			if (worstEstimate < 1/breakAt) {

				i = numberOfCandidates;
				break ;
			}
		}

	}

	return(1.0/worstEstimate);
}

#ifdef OLD
float heuristicMultiplePathNewlowerBound(t_graph * graph, t_array * paths, float breakAt) {

	long i, j, k;
	int numberOfPaths, numberOfNodes;
	t_array * path;
	unsigned long prev, current, next, nextNext, nextNextNext;
	int worstClique, currentClique; // TODO: generalize the type.
	unsigned long indexInOtherPath;
	t_weight weightLink1, weightLink2;

	arrayClear(outputWeights);
	arrayClear(inputWeights);

	numberOfPaths = arrayLength(paths);

	if (breakAt < INFINITY) breakAt *= numberOfPaths;

	for (i = 0; i < numberOfPaths; i++) {
		
		path = arrayGet(paths, i);
		numberOfNodes = arrayLength(path);
		for (j = 0; j < numberOfNodes; j++) {

			current = (unsigned long) arrayGet(path, j);

			if (arrayGet(outputWeights, current) == 0 && arrayGet(inputWeights, current) == 0) {

				arrayClear(arrayGet(indexes, current));
//printf("Clearing index for node %d\n", current);
			}

			arraySet(arrayGet(indexes, current), i, (void *) (j + 1));

			if (j < numberOfNodes - 1) {

				next = (unsigned long) arrayGet(path, j + 1);
				arrayIncBy(outputWeights, current, graphGetCost(graph, current, next));
			}
			if (j > 0) {

				prev = (unsigned long) arrayGet(path, j - 1);
				arrayIncBy(inputWeights, current, graphGetCost(graph, prev, current));
			}
		}
	}

	worstClique = 0;
	for (i = 0; i < numberOfPaths; i++) {
		
		path = arrayGet(paths, i);
		numberOfNodes = arrayLength(path) - 3;

		switch(numberOfNodes) {

			case -1:

				/*
				 * TODO: this can be improved.
				 */
				currentClique = (unsigned long) arrayGet(outputWeights, (unsigned long) arrayGet(path, 0)) +
						(unsigned long) arrayGet(outputWeights, (unsigned long) arrayGet(path, 1));
//printf("currentClique = %i\n", currentClique);
				if (currentClique > worstClique) {
				
					worstClique = currentClique;
					if (worstClique >= breakAt) return(((float) worstClique) / ((float) numberOfPaths));
				}
				break ;

			case 0:

				/*
				 * TODO: this can be improved.
				 */
				current = (unsigned long) arrayGet(path, 0);
				next = (unsigned long) arrayGet(path, 1);
				nextNext = (unsigned long) arrayGet(path, 2);

				currentClique = (unsigned long) arrayGet(outputWeights, current) +
						(unsigned long) arrayGet(outputWeights, next);

				for (k = 0; k < numberOfPaths; k++) {

					if (k == i) continue ;

					do {

						indexInOtherPath = (unsigned long) arrayGet(arrayGet(indexes, nextNext), k);
						if (indexInOtherPath == 0) break ;
						if ((unsigned long) arrayGet(arrayGet(indexes, next), k) != indexInOtherPath + 1) break ;
						if ((unsigned long) arrayGet(arrayGet(indexes, current), k) != indexInOtherPath + 2) break ;

						weightLink1 = graphGetCost(graph, next, nextNext);
						weightLink2 = graphGetCost(graph, next, current);
						if (weightLink1 > weightLink2)
							currentClique -= weightLink2;
						else
							currentClique -= weightLink1;

						currentClique += graphGetCost(graph, nextNext, next);
						k = numberOfPaths;
					} while(0);
				}
//printf("currentClique = %i\n", currentClique);
				if (currentClique > worstClique) {

					worstClique = currentClique;
					if (worstClique >= breakAt) return(((float) worstClique) / ((float) numberOfPaths));
				}
				break ;

			default:

			for (j = 0; j < numberOfNodes; j++) {
//printf("Number of nodes = %d, j = %u\n", numberOfNodes, j);
				next = (unsigned long) arrayGet(path, j + 1);
				nextNext = (unsigned long) arrayGet(path, j + 2);
				nextNextNext = (unsigned long) arrayGet(path, j + 3);
				currentClique = (unsigned long) arrayGet(inputWeights, next) + 
						(unsigned long) arrayGet(outputWeights, next) + 
						(unsigned long) arrayGet(outputWeights, nextNext);

				if (graphGetCost(graph, nextNextNext, next) < GRAPH_INFINITY)
					currentClique += (unsigned long) arrayGet(outputWeights, nextNextNext);

//printf("path = %d, currentNode = %lu, next = %lu, nextNext = %lu\n", path, arrayGet(path, j), next, nextNext);
//fflush(stdout);
				/*
				 * Subtract links already considered by the 'next', i.e.,
				 * links from nextNext to next in other paths.
				 */
				for (k = 0; k < numberOfPaths; k++) {

					if (k == i) continue ;

					do {

						indexInOtherPath = (unsigned long) arrayGet(arrayGet(indexes, nextNext), k);

						if (indexInOtherPath == 0) break ;
						if (arrayLength(arrayGet(paths, k)) == indexInOtherPath) break ;

						if (((unsigned long) arrayGet(arrayGet(paths, k), indexInOtherPath)) == next) {

							currentClique -= graphGetCost(graph, nextNext, next);

							/*
							 * Test the possibility of coding.
							 */
							if (arrayLength(arrayGet(paths, k)) > indexInOtherPath + 1) {

								current = (unsigned long) arrayGet(path, j);
								if (((unsigned long) arrayGet(arrayGet(paths, k), indexInOtherPath + 1)) == current) {

									weightLink1 = graphGetCost(graph, next, nextNext);
									weightLink2 = graphGetCost(graph, next, current);
									if (weightLink1 > weightLink2)
										currentClique -= weightLink2;
									else
										currentClique -= weightLink1;
								}
							}

							if (indexInOtherPath > 1) {

								if (((unsigned long) arrayGet(arrayGet(paths, k), indexInOtherPath - 2)) == nextNextNext) {

									weightLink1 = graphGetCost(graph, nextNext, next);
									weightLink2 = graphGetCost(graph, nextNext, nextNextNext);
									if (weightLink1 > weightLink2)
										currentClique -= weightLink2;
									else
										currentClique -= weightLink1;
								}
							}
						}

					} while(0);

					do {

						indexInOtherPath = (unsigned long) arrayGet(arrayGet(indexes, nextNextNext), k);

						if (indexInOtherPath == 0) break ;
						if (arrayLength(arrayGet(paths, k)) == indexInOtherPath) break ;

						if (((unsigned long) arrayGet(arrayGet(paths, k), indexInOtherPath)) == next)
							currentClique -= graphGetCost(graph, nextNextNext, next);

					} while(0);
				}

				if (currentClique > worstClique) {

					worstClique = currentClique;
					if (worstClique >= breakAt) return(((float) worstClique) / ((float) numberOfPaths));
				}

			}
		}
	}

	return(((float) worstClique) / ((float) numberOfPaths));
}
#endif

float heuristicSinglePathLowerBound(t_graph * graph, t_array * path) {

	int pathLength;
	t_weight cost, link1, link2, link3;
	int i;

	cost = 0;
	pathLength = arrayLength(path);
	if (pathLength <= 4) {

		for (i = 1; i < pathLength; i++) cost += graphGetCost(graph, (long) arrayGet(path, i-1), (long) arrayGet(path, i));
		return(cost);
	}

	link1 = graphGetCost(graph, (long) arrayGet(path, 0), (long) arrayGet(path, 1));
	link2 = graphGetCost(graph, (long) arrayGet(path, 1), (long) arrayGet(path, 2));
	link3 = graphGetCost(graph, (long) arrayGet(path, 2), (long) arrayGet(path, 3));

	cost = link1 + link2 + link3;
	for (i = 4; i < pathLength; i++) {

		link1 = link2;
		link2 = link3;
		link3 = graphGetCost(graph, (long) arrayGet(path, i-1), (long) arrayGet(path, i));
		if (link1 + link2 + link3 > cost) cost = link1 + link2 + link3;
	}

	return(cost);
}

float heuristicMultiplePathInterFlowLowerBound(t_graph * graph, t_array * paths) {

	t_array * path;
	int i;
	int numberOfPaths;
	float harmonic_sum;

	harmonic_sum = 0.0;
	numberOfPaths = arrayLength(paths);
	for (i = 0; i < numberOfPaths; i++) {

		path = arrayGet(paths, i);
		harmonic_sum += 1/heuristicSinglePathLowerBound(graph, path);
	}

	return(1/harmonic_sum);
}

float heuristicSinglePathUpperBound(t_graph * graph, t_array * path) {

	t_weight cost;
	int i, pathLength;

	cost = 0;
        pathLength = arrayLength(path);
	for (i = 1; i < pathLength; i++) {
	
		cost += graphGetCost(graph, (long) arrayGet(path, i-1), (long) arrayGet(path, i));
	}

	return(cost);
}

float heuristicMultiplePathInterFlowUpperUpperBound(t_graph * graph, t_array * paths) {

	t_array * path;
	int i;
	int numberOfPaths;
	float sum;

	sum = 0.0;
	numberOfPaths = arrayLength(paths);
	for (i = 0; i < numberOfPaths; i++) {

		path = arrayGet(paths, i);
		sum += heuristicSinglePathUpperBound(graph, path);
	}

	return(sum);
}

float heuristicMultiplePathInterFlowLowerUpperBound(t_graph * graph, t_array * paths) {

	t_array * path;
	int i;
	int numberOfPaths;
	float harmonic_sum;

	harmonic_sum = 0.0;
	numberOfPaths = arrayLength(paths);
	for (i = 0; i < numberOfPaths; i++) {

		path = arrayGet(paths, i);
		harmonic_sum += 1.0/heuristicSinglePathLowerBound(graph, path);
	}

	return(1.0/harmonic_sum);
}

float heuristicEstimateCost(t_graph * graph, t_array * paths) {

	return(heuristicMultiplePathInterFlowUpperUpperBound(graph, paths) / 3);
}

