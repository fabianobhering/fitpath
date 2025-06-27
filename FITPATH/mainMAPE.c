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
#include "simulationh2.h"
#include "memory.h"
#include "heuristics.h"
#include "dijkstra.h"
#include <time.h>

void printCurrentPaths(t_array * paths, int numberOfPairs, int numberOfDescriptors){  
	printf("currentPaths\n");
    for (int i = 0; i < numberOfPairs*numberOfDescriptors; i++) { //
   		for (int j = 0; j < arrayLength(arrayGet(paths, i)); j++) {
            printf("%lu ",(unsigned long) arrayGet(arrayGet(paths, i), j));
     }
     printf("\n");
    }    
}


void printSolution(t_array * paths[], int numberOfPairs, int numberOfDescriptors, t_graph * graph ){  
    FILE *arq_ns3, *arq_result;
    char arq_name[25];
	sprintf(arq_name,"../inst/delay/4s-pathCost");
  
    arq_ns3 = fopen(arq_name, "a+");
    

		
		for (int i = 0; i < numberOfPairs*numberOfDescriptors; i++) { // Unir os caminhos para simular 
			double pathCost=0;
			for (int j = 0; j < arrayLength(arrayGet(paths, i)); j++) {
			
				if(j>0){
					
					pathCost +=  (double) graphGetCost(graph, arrayGet(arrayGet(paths, i), j-1), arrayGet(arrayGet(paths, i), j));
				}
			}
			fprintf(arq_ns3,"%.2f\n", pathCost/(double) GRAPH_MULTIPLIER);
		
		}    

		
    fclose(arq_ns3);
    
}


int main(int argc, char ** argv) {

	int * currentSrc, * currentDst, * currentFlt, * randSrc, * randDst;
	int i, j, c, numberOfPairs, numberOfNodes;
	int numhist, numPaths;
    int *num;

	int numberOfDescriptors = 1; //quantidade de descritores alterado de 2 para 1 em 02/07/2023
	
	t_graph * graph;
	t_list * pathList;
	t_list * src, * dst, ** flt;
	t_prefixTreeNode * path;
	t_array * nodePairs, * flowTime, * simFlowTime, * txDurations;
	t_array * currentPaths;
	t_array * currentAuxPaths, * neighborPaths, * bestPaths, * histPaths;
	float  bestCost, bestDelay, currentCost, pertCost, bestTime, currentTime;
	clock_t t; //variável para armazenar tempo
    t_return * r, * rf;
	FILE *arq;

	
	graph = parserParse(argv[1], & src, & dst, &flt);
    
	nodePairs = arrayNew(listLength(src)); // Vetor que armazenar todos caminhos do conjunto de soluções em cada fluxo
	flowTime = arrayNew(listLength(flt)); // Vetor que armazenar todos intevalos de tempo de cada fluxo
	txDurations = arrayNew(listLength(flt)); // vetro que armazena as taxas do backoff.

	numberOfNodes = graphSize(graph); //Quantidade de nós
	
	i = 0;
	listBegin(src);
	listBegin(dst);
	MALLOC(randSrc, sizeof(int));
	MALLOC(randDst, sizeof(int));
	srand(time(NULL));
	* randDst = rand()%numberOfNodes;
	while(1) { // Gera o conjunto de soluções S para cada Fluxo
		currentSrc = listCurrent(src); // lista de fonte do arquivo
		currentDst = listCurrent(dst); //lista de destino do arquivo
		i++;		
		if (listNext(src) == NULL) break ;
		listNext(dst);
		
	}

	numberOfPairs = i; // F = quantidade de Fontes
	//printf("number of pair: %d\n", i);

	//numberOfPairs = 2;
	//numberOfDescriptors =2;
	numPaths = numberOfDescriptors*numberOfPairs;

	currentPaths = arrayNew(4);
	t_array * pathEval;
	pathEval = arrayNew(5);
	arraySet(pathEval, 0, 1);
	arraySet(pathEval, 1, 49);
	arraySet(pathEval, 2, 26);
	arraySet(pathEval, 3, 55);
	arraySet(pathEval, 4, 0);
	

	arraySet(currentPaths, 0, pathEval);

	pathEval = arrayNew(5);
	arraySet(pathEval, 0, 1);
	arraySet(pathEval, 1, 49);
	arraySet(pathEval, 2, 26);
	arraySet(pathEval, 3, 55);
	arraySet(pathEval, 4, 0);
	arraySet(currentPaths, 1, pathEval);

	pathEval = arrayNew(5);
	arraySet(pathEval, 0, 1);
	arraySet(pathEval, 1, 49);
	arraySet(pathEval, 2, 26);
	arraySet(pathEval, 3, 55);
	arraySet(pathEval, 4, 0);
	arraySet(currentPaths, 2, pathEval);

	pathEval = arrayNew(5);
	arraySet(pathEval, 0, 1);
	arraySet(pathEval, 1, 49);
	arraySet(pathEval, 2, 26);
	arraySet(pathEval, 3, 55);
	arraySet(pathEval, 4, 0);
	arraySet(currentPaths, 3, pathEval);

	simFlowTime = arrayNew(4); 
	arraySet(simFlowTime, 0,153657); 
	arraySet(simFlowTime, 1,113593); 
	arraySet(simFlowTime, 2,153657); 
	arraySet(simFlowTime, 3,113593); 

	// atribui o tempo de transmissao de um frame (ver tabela .xls)
	arraySet(txDurations, 0, 238); 
	arraySet(txDurations, 1, 238); 
	arraySet(txDurations, 2, 238); 
	arraySet(txDurations, 3, 238); 

	printf("currentPaths\n");
    for (i = 0; i < 4; i++) { // Unir os caminhos para simular 
   		for (j = 0; j < arrayLength(arrayGet(currentPaths, i)); j++) {
            printf("%lu ",(unsigned long) arrayGet(arrayGet(currentPaths, i), j));
     }
     printf(" - FLT %d \n", arrayGet(simFlowTime, i));
    }    

	MALLOC(r, sizeof(t_return));
	MALLOC(rf, sizeof(t_return));
    r = simulationSimulate(graph, currentPaths, simFlowTime, txDurations ); //função objetivo
    bestCost = r->cost;
	bestDelay = r->delay;
    currentTime = ((float)clock() - t)/((CLOCKS_PER_SEC/1000));
    bestTime = currentTime;
    rf=r; // melhor fluxo retornado
    printf(" EvalPath Cost = %f %f\n", bestCost, bestDelay);
	for (int f = 0; f < numberOfPairs*numberOfDescriptors; f++) {
		printf("Flow %lu %.2f - Delay %.2f \n",f, r->rateFlows[f], r->delayFlows[f] );	
	}

    arrayFree(currentPaths);
    free(currentPaths);
	free(rf);
	listFreeWithData(dst);
	free(dst);
	listFreeWithData(src);
	free(src);
	listFreeWithData(flt);
	free(flt);
	graphFree(graph);
	free(graph);
	

	arrayFree(nodePairs);
	free(nodePairs);

	arrayFree(flowTime);
	free(flowTime);

	arrayFree(simFlowTime);
	free(simFlowTime);

	arrayFree(txDurations);
	free(txDurations);

	
	return(0);
}
