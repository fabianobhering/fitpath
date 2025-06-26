#ifndef __HEURISTIC_H__
#define __HEURISTIC_H__

void heuristicInit(int numberOfNodes, int numberOfPaths);
void heuristicFinalize();
float heuristicMultiplePathNewlowerBound(t_graph * graph, t_array * paths, float * costs, float breakAt);
//float heuristicMultiplePathNewlowerBound(t_graph * graph, float * paths, float breakAt);
float heuristicSinglePathLowerBound(t_graph * graph, t_array * path);
float heuristicMultiplePathInterFlowLowerBound(t_graph * graph, t_array * paths);
float heuristicSinglePathUpperBound(t_graph * graph, t_array * path);
float heuristicMultiplePathInterFlowUpperUpperBound(t_graph * graph, t_array * paths);
float heuristicMultiplePathInterFlowLowerUpperBound(t_graph * graph, t_array * paths);
float heuristicEstimateCost(t_graph * graph, t_array * paths);


#endif

