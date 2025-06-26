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

int REF = 0;
int INST=0;

void printILSPaths(t_array * paths[], int numberOfPairs,int numberOfDescriptors ){
	for (int i = 0; i < numberOfPairs; i++) {
		for (int d = 0; d < numberOfDescriptors; d++) {
			for (int j = 0; j < arrayLength(arrayGet(paths[d], i)); j++) {
				printf("%lu ", (unsigned long) arrayGet(arrayGet(paths[d], i), j));
			}
			printf("\n");
		}
    }   
}



void printDSR(t_array * paths[], int numberOfPairs, int numberOfDescriptors, t_return * rf ){  
    
   
    /*
    printf("Log: Routes and Estimates %d\n", INST);
    
    FILE *arq_ns3;
    char arq_name[25];
	sprintf(arq_name,"data/routes/routesRandOut");
  	arq_ns3 = fopen(arq_name, "a+");
	
	if(numberOfPairs==12){
 
		for (int i = 0; i < numberOfPairs*numberOfDescriptors; i++) { // Unir os caminhos para simular 
			fprintf(arq_ns3,"if(flowId == %ld){",(INST*1000000)+i+1);	
			for (int j = arrayLength(arrayGet(paths, i))-1; j >=0 ; j--) {
				fprintf(arq_ns3,"	pathILS.push_back (Ipv4Address (\"10.0.0.%d\"));", (unsigned long) arrayGet(arrayGet(paths, i), j)+1);
			}
			
			fprintf(arq_ns3,"}\n");
		
		}    
		
		
		for (int i = 0; i < numberOfPairs*numberOfDescriptors; i++) { // Unir os caminhos para simular 
			fprintf(arq_ns3,"if(flowId == %ld){",(100*INST)+i+1);	
			for (int j = 0; j < arrayLength(arrayGet(paths, i)); j++) {
				fprintf(arq_ns3,"	pathILS.push_back (Ipv4Address (\"10.0.0.%d\"));", (unsigned long) arrayGet(arrayGet(paths, i), j)+1);
			}
			fprintf(arq_ns3,"}\n");
		
		}    
		
	}	
	fclose(arq_ns3);
	*/
	char arq_name[25];
	FILE *arq_result;
	sprintf(arq_name,"data/estimates/MATE-BACKOFF_GridInt_%d",numberOfPairs);
	arq_result = fopen(arq_name, "a+");
	for (int f = 0; f <  numberOfPairs*numberOfDescriptors; f++) {
		int steadyState = 0;
		if(rf->optimal->packetSentBetweenStates[f]>0) steadyState= 1;	
		fprintf(arq_result,"%d	%.2f	%.2f	%.4f	%d	%d\n", INST, rf->rateFlows[f], rf->packetsLossPerFlow[f], 0.054978* rf->delayFlows[f], arrayLength(arrayGet(paths, f)),steadyState);
	} 
    fclose(arq_result);
}


int comparePaths (t_array * paths[], int numberOfPairs, int numberOfDescriptors ){
for (int i = 0; i < numberOfPairs; i++) {	
	for (int d1 = 0; d1 < numberOfDescriptors-1; d1++) {
		for (int d2 = d1+1; d2 < numberOfDescriptors; d2++) {
			if(arrayGet(paths[d1], i)==arrayGet(paths[d2], i)){
			return 1;	
		}
		}
	}
}
return 0;
}

int main(int argc, char ** argv) {

	int * currentSrc, * currentDst, * currentFlt, * randSrc, * randDst;
	int i, j, c, numberOfPairs, numberOfNodes;
	
	int numberOfDescriptors = 1; 
	int numberOfPathsPerFlow = 5; 
	int numberOfFlows = atoi(argv[2]);
	t_graph * graph;
	t_list * pathList;
	t_list * src, * dst, ** flt;
	t_prefixTreeNode * path;
	t_array * nodePairs, * flowTimes, * txDurations;
	t_array * currentPaths,	* currentPathsILS[numberOfDescriptors], * currentPathsAuxILS[numberOfDescriptors], * currentPathsPerturbaILS[numberOfDescriptors], * bestPathsILS[numberOfDescriptors], * histPathsILS[numberOfDescriptors];
	float  bestCost, currentCost, pertCost, bestTime, currentTime;
	clock_t t; //variável para armazenar tempo
    t_return * r, * rf;
	FILE *arq;
	
	
	
	
	char* argInst = argv[1];
	int tam = strlen(argInst);

	
	graph = parserParse(argv[1], & src, & dst, &flt);
   //graphPrint(graph);
    //printf("graph size: %d ", graphSize(graph));
	/*
	 * Compute paths and place them in an array.
	 * Each array entry corresponds to a pair of
	 * source and destination. Each entry is a 
	 * handler for a list containing the paths.
	 */
	
	numberOfNodes = graphSize(graph); //Quantidade de nós
	
	
	
	
	int * l_sourceNode[3000][numberOfFlows], * l_destNode[3000][numberOfFlows], *l_flowTime[3000][numberOfFlows];
	
	FILE *arq_top;
    char arq_name[25];
	sprintf(arq_name,"data/scenarios/ScenarioRand");
  
    arq_top = fopen(arq_name, "r");
   
      char line[200]; 
	  char  *l_flow, *l_flows[3];
	  int sc=0;
      
      
      char p[numberOfFlows][100];
     
      
      while (fgets(line, sizeof(line), arq_top)) {
		
	
        //printf("The line is: %s\n", line); 
        
         if(numberOfFlows==3)
			sscanf(line, "%[^,],%[^,],%[^,]", &p[0], &p[1], &p[2]);
		if(numberOfFlows==6)
			sscanf(line, "%[^,],%[^,],%[^,],%[^,],%[^,],%[^,]", &p[0], &p[1], &p[2],&p[3],&p[4],&p[5]);
		if(numberOfFlows==9)
			sscanf(line, "%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,]", &p[0], &p[1], &p[2],&p[3],&p[4],&p[5],&p[6],&p[7],&p[8]);
		if(numberOfFlows==12)
			sscanf(line, "%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,]", &p[0], &p[1], &p[2],&p[3],&p[4],&p[5],&p[6],&p[7],&p[8],&p[9],&p[10],&p[11]);
		if(numberOfFlows==15)
			sscanf(line, "%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,]", &p[0], &p[1], &p[2],&p[3],&p[4],&p[5],&p[6],&p[7],&p[8],&p[9],&p[10],&p[11],&p[12],&p[13],&p[14]);
		if(numberOfFlows==18)
			sscanf(line, "%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,]", &p[0], &p[1], &p[2],&p[3],&p[4],&p[5],&p[6],&p[7],&p[8],&p[9],&p[10],&p[11],&p[12],&p[13],&p[14],&p[15],&p[16],&p[17]);
		
		
		//printf("%s %s %s\n", p[0],p[1],p[2]);
         
			
		for(int flows=0;flows<numberOfFlows;flows++){
		    
		    int p_flow[3];
		    sscanf(p[flows], "%d	%d	%d", &p_flow[0], &p_flow[1], &p_flow[2]);
		    			
			l_sourceNode[sc][flows]=p_flow[0];
			l_destNode[sc][flows]=p_flow[1];			 
			l_flowTime[sc][flows]=p_flow[2];
		}
		
		sc++;
		
	}
	fclose(arq_top);
	
	int * sourceNode, * destNode, *flowTime;
	int bf=1;
	
	sprintf(arq_name,"data/scenarios/instRand%d",numberOfFlows );
  
    arq_top = fopen(arq_name, "w+");
	
	float timeSimulation = 0;
	
	for(int scenario=383; scenario<500;scenario++){
		INST=scenario;
		
		t = clock(); //armazena tempo
		
		src = listNew();
		dst = listNew();
		flt = listNew();
		
		//printf("scenario: %d\n", scenario);
		for(int flows=0;flows<numberOfFlows;flows++){
			
			fprintf(arq_top, "%d	", l_sourceNode[scenario][flows] ); 
			MALLOC(sourceNode, sizeof(int));
			* sourceNode = l_sourceNode[scenario][flows];
			listAdd(src, sourceNode);
			
			
			fprintf(arq_top,"%d\n", l_destNode[scenario][flows] ); 
			MALLOC(destNode, sizeof(int));
			* destNode = l_destNode[scenario][flows];
			listAdd(dst, destNode);
			
			
			//printf( "FlowTime: %d\n", l_flowTime[scenario][flows] ); 
			MALLOC(flowTime, sizeof(int));
			* flowTime = l_flowTime[scenario][flows];
			listAdd(flt, flowTime);
		
		}
		
		
	
	    
		nodePairs = arrayNew(listLength(src)); // Vetor que armazenar todos caminhos do conjunto de soluções em cada fluxo
		flowTimes = arrayNew(listLength(flt)); 
		txDurations = arrayNew(listLength(flt)); // vetro que armazena as taxas do backoff.

		
		i = 0;
		listBegin(src);
		listBegin(dst);
		listBegin(flt);
		
		
		
		while(1) { // Gera o conjunto de soluções S para cada Fluxo
			
			currentFlt = listCurrent(flt);
			arraySet(flowTimes, i,(int) * currentFlt); // atribui o intevalo do fluxo
			//printf("flowTime  %d \n",  * currentFlt);
			arraySet(txDurations, i, 238); // atribui o tempo de transmissao de um frame (ver tabela .xls)
		
			i++;		
			if (listNext(flt) == NULL) break ;
			
		}
		
		//Novos valores para os fluxos
		
		arraySet(flowTimes, 0,551186); 
		arraySet(flowTimes, 1,291026); 
		arraySet(flowTimes, 2,173851); 
		if(numberOfFlows>=6){
			arraySet(flowTimes, 3,551186); 
			arraySet(flowTimes, 4,291026); 
			arraySet(flowTimes, 5,173851);
		}
		if(numberOfFlows>=9){
			arraySet(flowTimes, 6,551186); 
			arraySet(flowTimes, 7,291026); 
			arraySet(flowTimes, 8,173851);
		} 
		if(numberOfFlows>=12){
			arraySet(flowTimes, 9,551186); 
			arraySet(flowTimes, 10,291026); 
			arraySet(flowTimes, 11,173851); 
		}
		
		
		
		
		i=0;
		numberOfPathsPerFlow=5;
		while(1) { // Gera o conjunto de soluções S para cada Fonte/Destino
			
			currentSrc = listCurrent(src); // lista de fonte do arquivo
			currentDst = listCurrent(dst); //lista de destino do arquivo
			
			pathList = yen(graph, * currentSrc, * currentDst, numberOfPathsPerFlow); // gera a lista de todos os caminhos para cada fonte/destino, ordenado por menores caminhos
			
			arraySet(nodePairs, i, pathList); // atribui os caminhos ao fluxo
			
			//printf("%d paths were generated between nodes %d and %d:\n\n", listLength(pathList), * currentSrc, * currentDst);
			
			if(listLength(pathList)<numberOfPathsPerFlow) numberOfPathsPerFlow = listLength(pathList);
			
			//for (path = listBegin(pathList); path; path = listNext(pathList)) prefixTreePrint(path); //imprime os caminhos gerados para cada fluxos
			
			
			if(listLength(pathList)==0){
				 printf("0 paths were generate");
				 return(0);
				 
				}else if(listLength(pathList)<numberOfPathsPerFlow) numberOfPathsPerFlow = listLength(pathList);
			
			
			
			i++;		
			if (listNext(src) == NULL) break ;
			listNext(dst);
			
		}
		
		


		numberOfPairs = i; // F = quantidade de Fontes
		//printf("number of pair: %d\n", i);
		//fprintf(arq, "it	custo	vazao	tempo\n");
		
		
		/*
		 * Now comes the expensive part: we generate all combinations 
		 * of paths and simulate them, storing the best combination as
		 * we go.
		 */
		bestCost = INFINITY;
		currentPaths = arrayNew(numberOfPairs*numberOfDescriptors); 
		
		
		for (int d = 0; d < numberOfDescriptors; d++) {
			bestPathsILS[d] = arrayNew(numberOfPairs); 
			histPathsILS[d] = arrayNew(numberOfPairs); 
			currentPathsILS[d] = arrayNew(numberOfPairs); 
			currentPathsAuxILS[d] = arrayNew(numberOfPairs);
			currentPathsPerturbaILS[d] = arrayNew(numberOfPairs);
		}
		  
		
		
		for (i = 0; i < numberOfPairs; i++) { // atribui a solução inicial em s0
			
			path = listBegin(arrayGet(nodePairs, i)); // escolhe o caminho mais curto - primeiro
			
			for(int r = 0; r<numberOfPathsPerFlow-bf-i; r++){
				//printf("randPath: %d\n", r);
				
				path = listNext(arrayGet(nodePairs, i)); //escolhe aleatório entre os 5 melhores
			}
			arraySet(currentPathsILS[0], i, prefixTreePath(path));
			arraySet(currentPathsAuxILS[0], i, prefixTreePath(path));
			arraySet(currentPathsPerturbaILS[0], i, prefixTreePath(path));
			arraySet(bestPathsILS[0], i, prefixTreePath(path));
			arraySet(histPathsILS[0], i, prefixTreePath(path));
			
			for (int d = 1; d < numberOfDescriptors; d++) {
				//path = listNext(arrayGet(nodePairs, i)); //Forçar um caminhos diferentes
				arraySet(currentPathsILS[d], i, prefixTreePath(path));
				arraySet(currentPathsAuxILS[d], i, prefixTreePath(path));
				arraySet(currentPathsPerturbaILS[d], i, prefixTreePath(path));
				arraySet(bestPathsILS[d], i, prefixTreePath(path));
				arraySet(histPathsILS[d], i, prefixTreePath(path));
				//printf("path len: %d\n",listGetIndex(arrayGet(nodePairs, i)));  
			 }
		}
		//numberOfPathsPerFlow=numberOfPathsPerFlow-2;
		//printf("%d\n",numberOfPathsPerFlow);  
		
	   if (bf==5) bf=1; else bf++;
		
		
		//printf("S0\n");
		for (i = 0; i < numberOfPairs; i++) {
			for (int d = 0; d < numberOfDescriptors; d++) {
				for (j = 0; j < arrayLength(arrayGet(bestPathsILS[d], i)); j++) {
					//printf("%lu ", (unsigned long) arrayGet(arrayGet(bestPathsILS[d], i), j));
				}
				
			}
			
		}
		
		c = 0;
		for (i = 0; i < numberOfPairs; i++) { // Unir os caminhos para simular 
		   for (int d = 0; d < numberOfDescriptors; d++) {
				arraySet(currentPaths, c, arrayGet(currentPathsILS[d], i));
				c++;
		   }
		   
		}
		//printf("currentPaths\n");
		for (i = 0; i < numberOfPairs*numberOfDescriptors; i++) { // Unir os caminhos para simular 
			for (j = 0; j < arrayLength(arrayGet(currentPaths, i)); j++) {
				//printf("%lu ",(unsigned long) arrayGet(arrayGet(currentPaths, i), j));
				
			}
			//printf("%ld ",(long)arrayLength(arrayGet(currentPaths, i)));
			//printf("\n");
		
		}    
		
		MALLOC(r, sizeof(t_return));
		MALLOC(rf, sizeof(t_return));
		r = simulationSimulate(graph, currentPaths, flowTimes, txDurations); //função objetivo
		bestCost = r->cost;
		
		currentTime = ((float)clock() - t)/((CLOCKS_PER_SEC/1000));
		bestTime = currentTime;
	    //printf("Inst %d - Agreggate Throughput = %.2f\n", scenario, 1800000/ bestCost);
		//printf("%.3f\n", currentTime );
		
		timeSimulation +=currentTime;
		
		
		for (int f = 0; f < numberOfFlows; f++) {
			int steadyState = 0;
			if(r->optimal->packetSentBetweenStates[f]>0) steadyState= 1;	
			printf("%d	%.2f		%.2f	%.2f	%d	%d\n",scenario, r->rateFlows[f], r->packetsLossPerFlow[f], r->delayFlows[f], (long)arrayLength(arrayGet(currentPaths, f)), steadyState);
			
		}
		
		
	
		
		//printDSR(currentPaths, numberOfPairs, numberOfDescriptors,r);
	   
		
		
		
    }
  
    arrayFree(currentPaths);
    free(currentPaths);
	
	
    free(r);

	
	listFreeWithData(dst);
	free(dst);
	
	listFreeWithData(src);
	free(src);

	listFreeWithData(flt);
	free(flt);
	graphFree(graph);
	free(graph);
	
	arrayFree(txDurations);
	free(txDurations);

	
		
  fclose(arq_top);
  printf("mean time: %.3f\n", timeSimulation/500 );	
  return(0);
}
