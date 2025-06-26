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
int NumNodes=0;

void printCurrentPaths(t_array * paths, int numberOfPairs, int numberOfDescriptors){  
	printf("currentPaths\n");
    for (int i = 0; i < numberOfPairs*numberOfDescriptors; i++) { //
   		for (int j = 0; j < arrayLength(arrayGet(paths, i)); j++) {
            printf("%lu ",(unsigned long) arrayGet(arrayGet(paths, i), j));
     }
     printf("\n");
    }    
}


double maxCost(t_graph * graph, t_array * paths, t_array * flowTimes){
	//Avaliação prévia do custo máximo da solução, com base na probabilidade de entrega. Sem considerar as perdas por enfileiramento e interferência.
	int tmp;
	float deliveryProbability, maxInterval, maxCost, flowTime;
	for (int i = 0; i < arrayLength(paths); i++) {
		deliveryProbability = 1.0;
		for (int j = 1; j < arrayLength(arrayGet(paths, i)); j++) {
			tmp = 1 - sqrt((double) GRAPH_MULTIPLIER / (double) graphGetCost(graph, (long) arrayGet(arrayGet(paths, i), j - 1), (long) arrayGet(arrayGet(paths, i), j)));
			//printf("hopCost: %.2f ", tmp);
			deliveryProbability  *= (1 - tmp * tmp * tmp * tmp);
		}
		maxInterval = (int) arrayGet(flowTimes, i) * deliveryProbability;
		flowTime=(int) arrayGet(flowTimes, i);
		maxCost = maxCost + ((maxInterval-flowTime)/maxInterval);			

		//printf("\npathDeliveryProbability: %.2f\n", deliveryProbability);
		//printf("maxInterval: %.2f\n", maxInterval);
	}
	
	return maxCost;

}


// void printDSR(t_array * paths[], int numberOfPairs, int numberOfDescriptors, t_return * rf ){  
//     printf("DSR Routes\n");
//     //INST++;
//     FILE *arq_ns3, *arq_result;
//     char arq_name[25];
// 	sprintf(arq_name,"../inst/newILS/0new-route300_30_ref%d", REF);
  
//     arq_ns3 = fopen(arq_name, "w"); //"a+"
    
	 
// 		fprintf(arq_ns3,"if(INST == %d){\n",INST);	
// 		for (int i = 0; i < numberOfPairs*numberOfDescriptors; i++) { // Unir os caminhos para simular 
// 			fprintf(arq_ns3,"if(flowId == %ld){",10+i+1);	
// 			for (int j = arrayLength(arrayGet(paths, i))-1; j >=0 ; j--) {
// 				fprintf(arq_ns3,"	pathILS.push_back (Ipv4Address (\"10.0.0.%d\"));", (unsigned long) arrayGet(arrayGet(paths, i), j)+1);
// 			}
			
// 			fprintf(arq_ns3,"}\n");
		
// 		}    
		
// 		for (int i = 0; i < numberOfPairs*numberOfDescriptors; i++) { // Unir os caminhos para simular 
// 			fprintf(arq_ns3,"if(flowId == %ld){",i+1);	
// 			for (int j = 0; j < arrayLength(arrayGet(paths, i)); j++) {
// 				fprintf(arq_ns3,"	pathILS.push_back (Ipv4Address (\"10.0.0.%d\"));", (unsigned long) arrayGet(arrayGet(paths, i), j)+1);
// 			}
// 			fprintf(arq_ns3,"}\n");
		
// 		}    
// 		fprintf(arq_ns3,"}\n");
// 		/* 
// 		sprintf(arq_name,"ns3/ScenarioRandom3-ILS_Result_2");
// 		arq_result = fopen(arq_name, "a+");
// 		for (int f = 0; f <  numberOfPairs*numberOfDescriptors; f++) {

// 			//fprintf(arq_result, "%d	%.2f	%d	%.2f\n",INST, 1800000/rf->meanInterval,  f+1, 1800000/rf->meanIntervalFlow[f]);
// 			fprintf(arq_result, "%.2f	%.2f\n",1800000/rf->meanInterval,  1800000/rf->meanIntervalFlow[f]);
			
// 		}
// 		fclose(arq_result);
// 		*/
	
//     fclose(arq_ns3);
    
// }

void printDSR(t_array * paths[], int numberOfPairs, int numberOfDescriptors, t_return * rf ){  
    printf("DSR Routes\n");
    //INST++;
    FILE *arq_ns3, *arq_result;
    char arq_name[25];
	sprintf(arq_name,"../inst/newILS/0new-route300_%d-%d-ref%d", NumNodes, INST, REF);
  
    arq_ns3 = fopen(arq_name, "w"); //"a+"
    
	 
		//fprintf(arq_ns3,"if(INST == %d){\n",INST);	
		for (int i = 0; i < numberOfPairs*numberOfDescriptors; i++) { // Unir os caminhos para simular 
			//fprintf(arq_ns3);	//fprintf(arq_ns3,"if(flowId == %ld){",10+i+1);	
			for (int j = arrayLength(arrayGet(paths, i))-1; j >=0 ; j--) {
				fprintf(arq_ns3,"%d ", (unsigned long) arrayGet(arrayGet(paths, i), j));
			}
			
			fprintf(arq_ns3,"\n");
		
		}    
		
		// for (int i = 0; i < numberOfPairs*numberOfDescriptors; i++) { // Unir os caminhos para simular 
		// 	fprintf(arq_ns3,"if(flowId == %ld){",i+1);	
		// 	for (int j = 0; j < arrayLength(arrayGet(paths, i)); j++) {
		// 		fprintf(arq_ns3,"10.0.0.%d ", (unsigned long) arrayGet(arrayGet(paths, i), j)+1);
		// 	}
		// 	fprintf(arq_ns3,"}\n");
		
		// }    
		// fprintf(arq_ns3,"}\n");
		/* 
		sprintf(arq_name,"ns3/ScenarioRandom3-ILS_Result_2");
		arq_result = fopen(arq_name, "a+");
		for (int f = 0; f <  numberOfPairs*numberOfDescriptors; f++) {

			//fprintf(arq_result, "%d	%.2f	%d	%.2f\n",INST, 1800000/rf->meanInterval,  f+1, 1800000/rf->meanIntervalFlow[f]);
			fprintf(arq_result, "%.2f	%.2f\n",1800000/rf->meanInterval,  1800000/rf->meanIntervalFlow[f]);
			
		}
		fclose(arq_result);
		*/
	
    fclose(arq_ns3);
    
}



void printSolution(t_array * paths[], int numberOfPairs, int numberOfDescriptors, t_graph * graph ){  
    FILE *arq_ns3, *arq_result;
    char arq_name[25];
	sprintf(arq_name,"../inst/delay/4s-pathCost300_%d-%d-ref%d", NumNodes, INST, REF);
  
    arq_ns3 = fopen(arq_name, "a+");
    

		//fprintf(arq_ns3,"INST == %d\n",INST);	
		for (int i = 0; i < numberOfPairs*numberOfDescriptors; i++) { // Unir os caminhos para simular 
			double pathCost=0;
			for (int j = 0; j < arrayLength(arrayGet(paths, i)); j++) {
				//fprintf(arq_ns3,"%d	", (unsigned long) arrayGet(arrayGet(paths, i), j)+1);
				if(j>0){
					//printf("%.2f	", (double) graphGetCost(graph, arrayGet(arrayGet(bestPathsILS[d], i), j-1), arrayGet(arrayGet(bestPathsILS[d], i), j)));
					pathCost +=  (double) graphGetCost(graph, arrayGet(arrayGet(paths, i), j-1), arrayGet(arrayGet(paths, i), j));
				}
			}
			fprintf(arq_ns3,"%.2f\n", pathCost/(double) GRAPH_MULTIPLIER);
		
		}    
		//fprintf(arq_ns3,"\n");
		
    fclose(arq_ns3);
    
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
	int numhist, numPaths;
    int *num;

	int numberOfDescriptors = 1; //quantidade de descritores alterado de 2 para 1 em 02/07/2023
	int numberOfPathsPerFlow = 100; // S = conjunto de soluções para cada fluxo
   
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
	
    /* s = FxP
       s = nodePairs
       F = numberOfPairs
       P = currentPaths1[F] e currentPaths2[F]
     
     s = F1    | currentPaths1 currentPaths2 |
         F2    | currentPaths1 currentPaths2 |
     */
   
	/*INST = atoi(argv[3]);
	REF = atoi(argv[2]);*/

	REF = atoi(argv[4]);
	INST = atoi(argv[3]);
	NumNodes = atoi(argv[2]); //Alterado em 18/07/2023 por Debora
	
	


	graph = parserParse(argv[1], & src, & dst, &flt);
    //graphPrint(graph);
	/*
	 * Compute paths and place them in an array.
	 * Each array entry corresponds to a pair of
	 * source and destination. Each entry is a 
	 * handler for a list containing the paths.
	 */
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
		
		//* randSrc = rand()%numberOfNodes;
		//currentSrc = randSrc; //Fonte Aleatória
		
		//* randSrc = INST+i;
		//currentSrc = randSrc; //Fonte Aleatória
		
		currentSrc = listCurrent(src); // lista de fonte do arquivo
		currentDst = listCurrent(dst); //lista de destino do arquivo
		//currentDst = randDst;// Destino Aleatório
		pathList = yen(graph, * currentSrc, * currentDst, numberOfPathsPerFlow); // gera a lista de todos os caminhos para cada fluxos, ordenado por menores caminhos
		arraySet(nodePairs, i, pathList); // atribui os caminhos ao fluxo
		//currentFlt = listCurrent(flt);
		//arraySet(flowTime, i, * currentFlt); // atribui o intevalo do fluxo
		
        printf("%d paths were generated between nodes %d and %d:\n\n", listLength(pathList), * currentSrc, * currentDst);
		
		if(listLength(pathList)<numberOfPathsPerFlow) numberOfPathsPerFlow = listLength(pathList);
       // for (path = listBegin(pathList); path; path = listNext(pathList)) prefixTreePrint(path); //imprime os caminhos gerados para cada fluxos
        
        if(listLength(pathList)==0){
			 //fprintf(arq, "0	0	0	0\n");
			 //fclose(arq);
			 return(0);
			 
			}else if(listLength(pathList)<numberOfPathsPerFlow) numberOfPathsPerFlow = listLength(pathList);
       	
		
		
		i++;		
		if (listNext(src) == NULL) break ;
		listNext(dst);
		//listNext(flt);
	}

	listBegin(flt);
	int f = 0;
	while(1) { // Gera o conjunto de soluções S para cada Fluxo
		
		currentFlt = listCurrent(flt);
		arraySet(flowTime, f, * currentFlt); // atribui o intevalo do fluxo
		arraySet(txDurations, f, 238); // atribui o tempo de transmissao de um frame (ver tabela .xls)
		//printf("flow %d - %d\n", f, * currentFlt);
		f++;		
		if (listNext(flt) == NULL) break ;
	}
	

	numberOfPairs = i; // F = quantidade de Fontes
	//printf("number of pair: %d\n", i);
	//fprintf(arq, "it	custo	vazao	tempo\n");
    
	



//********* Avaliar um caminho específico *****************
//*****Para Determinar uma quantidade específica de fontes e fluxos
//numberOfPairs = 2;
//numberOfDescriptors =2;
numPaths = numberOfDescriptors*numberOfPairs;


	// //Novos valores para os fluxos   comentado em 02/07/2023
	// //MDC - BUS
	// arraySet(flowTime, 0,72756); 
	// arraySet(flowTime, 1,97008); 
	// arraySet(flowTime, 2,72756); 
	// arraySet(flowTime, 3,97008); 
	// //GOP - BUS
	// arraySet(flowTime, 0,153657); 
	// arraySet(flowTime, 1,113593); 
	// arraySet(flowTime, 2,153657); 
	// arraySet(flowTime, 3,113593); 

//REF 1-1 - Cost 0.000400 2.144434
//1 10 19 48 11 0
//2 7 24 54 0
//Flow 0 999.90 - Delay 146.07  
//Flow 1 999.90 - Delay 191.98 
//Flow 2 999.90 - Delay 126.06 
//Flow 3 999.90 - Delay 179.22 
//Flow 0 145527.58 - Delay 146.07 
//Flow 1 145527.53 - Delay 191.98 
//Flow 2 145527.58 - Delay 126.06 
//Flow 3 145527.53 - Delay 179.22 


//REF 5-1 
//1 10 19 48 43 0
//2 7 55 54 0
//EvalPath Cost = 0.000400 2.085258
//Flow 0 999.90 - Delay 121.55 
//Flow 1 999.90 - Delay 187.53 
//Flow 2 999.90 - Delay 141.72 
//Flow 3 999.90 - Delay 174.78 
//Flow 0 145527.58 - Delay 121.55 
//Flow 1 145527.53 - Delay 187.53 
//Flow 2 145527.58 - Delay 141.72 
//Flow 3 145527.53 - Delay 174.78
/*
	currentPaths = arrayNew(4);
	t_array * pathEval;
	pathEval = arrayNew(6);
	arraySet(pathEval, 0, 1);
	arraySet(pathEval, 1, 10);
	arraySet(pathEval, 2, 19);
	arraySet(pathEval, 3, 48);
	arraySet(pathEval, 4, 43);
	arraySet(pathEval, 5, 0);

	arraySet(currentPaths, 0, pathEval);

	pathEval = arrayNew(6);
	arraySet(pathEval, 0, 1);
	arraySet(pathEval, 1, 10);
	arraySet(pathEval, 2, 19);
	arraySet(pathEval, 3, 48);
	arraySet(pathEval, 4, 43);
	arraySet(pathEval, 5, 0);
	arraySet(currentPaths, 1, pathEval);

	pathEval = arrayNew(5);
	arraySet(pathEval, 0, 2);
	arraySet(pathEval, 1, 7);
	arraySet(pathEval, 2, 55);
	arraySet(pathEval, 3, 54);
	arraySet(pathEval, 4, 0);
	arraySet(currentPaths, 2, pathEval);

	pathEval = arrayNew(5);
	arraySet(pathEval, 0, 2);
	arraySet(pathEval, 1, 7);
	arraySet(pathEval, 2, 55);
	arraySet(pathEval, 3, 54);
	arraySet(pathEval, 4, 0);
	arraySet(currentPaths, 3, pathEval);

	simFlowTime = arrayNew(4); 
	arraySet(simFlowTime, 0, arrayGet(flowTime, 0));
	arraySet(simFlowTime, 1, arrayGet(flowTime, 1));
	arraySet(simFlowTime, 2, arrayGet(flowTime, 2));
	arraySet(simFlowTime, 3, arrayGet(flowTime, 3));


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
return(0);

*/
//**************************************************************


	/*
	 * Now comes the expensive part: we generate all combinations 
	 * of paths and simulate them, storing the best combination as
	 * we go.
	 */
	bestCost = INFINITY;
	bestDelay = INFINITY;
	currentPaths = arrayNew(numberOfPairs*numberOfDescriptors); 
	currentAuxPaths = arrayNew(numberOfPairs*numberOfDescriptors); 
    neighborPaths = arrayNew(numberOfPairs*numberOfDescriptors);
	bestPaths = arrayNew(numberOfPairs*numberOfDescriptors);
	/*for (int i = 0; i < numberOfPathsPerFlow; i++) {
		histPaths[i] = arrayNew(numberOfPairs*numberOfDescriptors);
	}
	*/
	histPaths = arrayNew(numberOfPairs*numberOfDescriptors);
	simFlowTime = arrayNew(numberOfPairs*numberOfDescriptors); 
	
	
	c=0;
    for (i = 0; i < numberOfPairs; i++) { // Sulução Inicial
       path = listBegin(arrayGet(nodePairs, i)); 
       for (int d = 0; d < numberOfDescriptors; d++) {
		    arraySet(bestPaths, c, prefixTreePath(path));
			arraySet(currentPaths, c, prefixTreePath(path));
			arraySet(histPaths, c, prefixTreePath(path));
			arraySet(simFlowTime, c, arrayGet(flowTime, c));
			c++;
	   } 
    }

	printCurrentPaths(currentPaths, numberOfPairs, numberOfDescriptors);
	
	//printDSR(currentPaths, numberOfPairs, numberOfDescriptors, rf);
	//printSolution(currentPaths, numberOfPairs, numberOfDescriptors, graph);
	//return(0);

	MALLOC(r, sizeof(t_return));
	MALLOC(rf, sizeof(t_return));
    r = simulationSimulate(graph, currentPaths, simFlowTime, txDurations); //função objetivo
    bestCost = r->cost;
	bestDelay = r->delay;
    currentTime = ((float)clock() - t)/((CLOCKS_PER_SEC/1000));
    bestTime = currentTime;
    rf=r; // melhor fluxo retornado
    printf("S0 BestCost = %.4f\n", bestCost);
    
	//FILE *arq_delay;
   // char arq_name[25];
	//sprintf(arq_name,"../inst/delay/4s-rate.txt");
	//arq_delay = fopen(arq_name, "a+");
	for (int f = 0; f < numberOfPairs*numberOfDescriptors; f++) {
		//printf("Flow %lu %.2f - Delay %.2f \n",f, r->meanIntervalPerFlow[f], r->delayFlows[f] );	
		printf("%d	%d	%.2f	%.2f	%d\n",INST, f, r->rateFlows[f], r->delayFlows[f],arrayLength(arrayGet(currentPaths, f)) );	
		//fprintf(arq_delay,"%.2f\n", r->meanIntervalPerFlow[f] );	
	}
	



	
	
    


	//printf("Busca Local 0\n");
    c=0;
    for (i = 0; i < numberOfPairs; i++) { // obter o vizinho mais próximo de cada caminho
       path = listNext(arrayGet(nodePairs, i)); 
	   //Fazer a poda aqui.
       for (int d = 0; d < numberOfDescriptors; d++) {
			arraySet(neighborPaths, c, prefixTreePath(path));
			arraySet(currentAuxPaths, c, arrayGet(currentPaths, c));
			c++;
	   } 
    }
	
    num = (int *)calloc(numPaths+1, sizeof(int)) ;
    if ( num == NULL ) {
        perror("malloc") ;
        return -1;
    }

	numhist=0;
    while ( num[numPaths] == 0 ) { //Permutação entre os caminhos
		int newPaths =0;
		for(i=0; i < 2; i++) { //permutar os caminhos com o 2 conjuntos (currentPaths e neighborPaths)
            for(j=0; j < numPaths; j++) {
                if(num[j]){
                    //printf("%d-n ", j);
					newPaths=1;
					arraySet(currentPaths, j, arrayGet(neighborPaths, j));
                }else{
                    //printf("%d-c ", j);
					arraySet(currentPaths, j, arrayGet(currentAuxPaths, j));
                }
            }
			num[0]++;
           // putchar('\n');
			//Avaliar a solução caso tenha um novo caminho
			
	
			if(newPaths){
				//printCurrentPaths(currentPaths, numberOfPairs, numberOfDescriptors);
				//Avaliação prévia da solução.	
				currentCost = maxCost(graph, currentPaths, simFlowTime);
				//printf("maxCost = %.4f\n", currentCost);
				if (currentCost <= bestCost) { //Executa a simulação se tiver melhor ou igual custo na avaliação prévia 
				
					r = simulationSimulate(graph, currentPaths, simFlowTime, txDurations); //função objetivo
					currentCost = r->cost;
					currentTime = ((float)clock() - t)/((CLOCKS_PER_SEC/1000));
					//printf("currentCost = %.4f\n", currentCost);	
					if (currentCost <bestCost) {
						
							bestTime = currentTime;
							bestCost = currentCost;
							bestDelay = r->delay;
							for(int p=0; p<numPaths;p++){ //armazena a solução anterior no histórico
								arraySet(histPaths, p, arrayGet(bestPaths, p));
							}

							for(int p=0; p<numPaths;p++){ //obtem a melhor solução
								arraySet(bestPaths, p, arrayGet(currentPaths, p));
							}

							printf("it 0 BestCost = %.4f\n", bestCost);	
							
							//printf("historico");
							//printCurrentPaths(histPaths, numberOfPairs, numberOfDescriptors);
							//printf("best");
							//printCurrentPaths(bestPaths, numberOfPairs, numberOfDescriptors);

					}
					
					if(currentCost ==bestCost){ //Critério de desempate
						printf("Empate = %f %f\n", bestCost, r->delay);	
						//printCurrentPaths(currentPaths, numberOfPairs, numberOfDescriptors);
						for (int f = 0; f < numberOfPairs*numberOfDescriptors; f++) {
							printf("%d	%.2f	%d\n", f, r->delayFlows[f],arrayLength(arrayGet(currentPaths, f)) );	
						}
						
						if(r->delay < bestDelay){
							
							bestTime = currentTime;
							bestCost = currentCost;
							bestDelay = r->delay;
							for(int p=0; p<numPaths;p++){ //armazena a solução anterior no histórico
								arraySet(histPaths, p, arrayGet(bestPaths, p));
							}

							for(int p=0; p<numPaths;p++){ //obtem a melhor solução
								arraySet(bestPaths, p, arrayGet(currentPaths, p));
							}

							printf("it 0 desempate BestCost = %.4f\n", bestCost);	
							for (int f = 0; f < numberOfPairs*numberOfDescriptors; f++) {
								printf("Flow %lu %.2f - Delay %.2f \n",f, r->rateFlows[f], r->delayFlows[f] );	
							}
	
						}
						

					}
						
				}
			}
			
        }

        for(i=0; i < numPaths; i++) {
            if(num[i] == 2) {
                num[i] = 0;
                num[i+1]++ ;
            }
        }
    }
 
    free(num) ;
	
   
	//Cada iteração: Permuta a proxima solução com a melhor solução e em seguida com o histórico.
	int iteracao =1;
    while (bestCost>0 & iteracao < numberOfPathsPerFlow-1 & currentTime<60000){ //Reduzir o time para instancias que não atingirem o tempo
		int bestSolution=0;
		//printf("iteração %d\n", iteracao);
		
		//printf("Permutar a melhor solução com a próxima solução.\n");
		//Perturbação com a Melhor Solução.
		for(int p=0; p<numPaths;p++){ //obter a melhor solução
			arraySet(currentAuxPaths, p, arrayGet(bestPaths, p));
		}
		 
		c=0;
		for (i = 0; i < numberOfPairs; i++) { // obter a próxima solução vizinha.
			path = listNext(arrayGet(nodePairs, i)); 
			for (int d = 0; d < numberOfDescriptors; d++) {
					arraySet(neighborPaths, c, prefixTreePath(path));
					c++;
			} 
		}
		//printf("best");
		//printCurrentPaths(currentAuxPaths, numberOfPairs, numberOfDescriptors);
		//printf("neighbor");
		//printCurrentPaths(neighborPaths, numberOfPairs, numberOfDescriptors);
		
		//printf("Busca Local permuta 1\n");
		num = (int *)calloc(numPaths+1, sizeof(int)) ;
		if ( num == NULL ) {
			perror("malloc") ;
			return -1;
		}
		while ( num[numPaths] == 0 ) { //Permutação entre os caminhos
			int newPaths=0;
			for(i=0; i < 2; i++) { //permutar os caminhos com o 2 conjuntos (currentPaths e neighborPaths)
				for(j=0; j < numPaths; j++) {
					if(num[j]){
						//printf("%d-n ", j);
						newPaths=1;
						arraySet(currentPaths, j, arrayGet(neighborPaths, j));
					}else{
						//printf("%d-c ", j);
						arraySet(currentPaths, j, arrayGet(currentAuxPaths, j));
					}
				}
				num[0]++ ;
			// putchar('\n');
				
				if(newPaths){
					//Avalia o caminho
					currentCost = maxCost(graph, currentPaths, simFlowTime);
					if (currentCost <= bestCost) { //Executa a simulação se tiver melhor ou igual custo na avaliação prévia 
				
						r = simulationSimulate(graph, currentPaths, simFlowTime, txDurations); //função objetivo
						currentCost = r->cost;
						currentTime = ((float)clock() - t)/((CLOCKS_PER_SEC/1000));
						//printf("currentCost = %.4f\n", currentCost);	
						if (currentCost <bestCost) {
							bestTime = currentTime;
							bestCost = currentCost;
							bestDelay = r->delay;
							
							bestSolution=1;
							
							for(int p=0; p<numPaths;p++){ //obtem a melhor solução
								arraySet(bestPaths, p, arrayGet(currentPaths, p));
							}

							printf("it %d BestCost = %f\n", iteracao, bestCost);	
							//printf("historico");
							//printCurrentPaths(histPaths, numberOfPairs, numberOfDescriptors);
							//printf("best");
							//printCurrentPaths(bestPaths, numberOfPairs, numberOfDescriptors);
							
						}

						if(currentCost ==bestCost){ //Critério de desempate
							printf("Empate = %f %f\n", bestCost, r->delay);	
							//printCurrentPaths(currentPaths, numberOfPairs, numberOfDescriptors);
							for (int f = 0; f < numberOfPairs*numberOfDescriptors; f++) {
								printf("%d	%.2f	%d\n", f, r->delayFlows[f],arrayLength(arrayGet(currentPaths, f)) );	
							}
							
							if(r->delay < bestDelay){
								
								bestTime = currentTime;
								bestCost = currentCost;
								bestDelay = r->delay;
								for(int p=0; p<numPaths;p++){ //armazena a solução anterior no histórico
									arraySet(histPaths, p, arrayGet(bestPaths, p));
								}

								for(int p=0; p<numPaths;p++){ //obtem a melhor solução
									arraySet(bestPaths, p, arrayGet(currentPaths, p));
								}

								printf("it %d desempate BestCost = %f\n", iteracao, bestCost);
								for (int f = 0; f < numberOfPairs*numberOfDescriptors; f++) {
								   printf("Flow %lu %.2f - Delay %.2f \n",f, r->rateFlows[f], r->delayFlows[f] );	
							    }
		
							}
							

					}



					}else{
						printf("maxCost = %.4f\n", currentCost);
					
					}
				}
			
				for(i=0; i < numPaths; i++) {
					if(num[i] == 2) {
						num[i] = 0;
						num[i+1]++ ;
					}
				}
			}
    	}
 
		free(num);

    	
		if(bestSolution){ //Caso tenha encontrado uma melhor solução, permutar essa solução com o histórico.
			//printf("Permutar a melhor solução da iteração com o histórico.\n");
			
			
			for(int p=0; p<numPaths;p++){ //armazena a solução anterior no histórico
				arraySet(currentAuxPaths, p, arrayGet(bestPaths, p));
			}
			for(int p=0; p<numPaths;p++){ //armazena a solução anterior no histórico
				arraySet(neighborPaths, p, arrayGet(histPaths, p));
			}


		}else{
			//printf("Permutar o histórico com a próxima solução.\n");
			for(int p=0; p<numPaths;p++){ //armazena a solução anterior no histórico
				arraySet(currentAuxPaths, p, arrayGet(histPaths, p));
			}
		}
		
		//printCurrentPaths(currentAuxPaths, numberOfPairs, numberOfDescriptors);
		//printCurrentPaths(neighborPaths, numberOfPairs, numberOfDescriptors);
		
		//printf("Busca Local permuta 2\n");
		num = (int *)calloc(numPaths+1, sizeof(int)) ;
		if ( num == NULL ) {
			perror("malloc") ;
			return -1;
		}
		while ( num[numPaths] == 0 ) { //Permutação entre os caminhos
			int newPaths=0;
			for(i=0; i < 2; i++) { //permutar os caminhos com o 2 conjuntos (currentPaths e neighborPaths)
				for(j=0; j < numPaths; j++) {
					if(num[j]){
						//printf("%d-n ", j);
						newPaths=1;
						arraySet(currentPaths, j, arrayGet(neighborPaths, j));
					}else{
						//printf("%d-c ", j);
						arraySet(currentPaths, j, arrayGet(currentAuxPaths, j));
					}
				}
				num[0]++ ;
			// putchar('\n');
				
				if(newPaths){
					//printCurrentPaths(currentPaths, numberOfPairs, numberOfDescriptors);
					//Avaliação prévia da solução.	
					currentCost = maxCost(graph, currentPaths, simFlowTime);
					//printf("maxCost = %.4f\n", currentCost);
					if (currentCost <= bestCost) { //Executa a simulação se tiver melhor ou igual custo na avaliação prévia 
				
						r = simulationSimulate(graph, currentPaths, simFlowTime, txDurations); //função objetivo
						currentCost = r->cost;
						currentTime = ((float)clock() - t)/((CLOCKS_PER_SEC/1000));
						//printf("currentCost = %.4f\n", currentCost);	
						if (currentCost <bestCost) {
							bestTime = currentTime;
							bestCost = currentCost;
							bestDelay = r->delay;
							for(int p=0; p<numPaths;p++){ //armazena a solução anterior no histórico
								arraySet(histPaths, p, arrayGet(bestPaths, p));
							}

							for(int p=0; p<numPaths;p++){ //obtem a melhor solução
								arraySet(bestPaths, p, arrayGet(currentPaths, p));
							}

							printf("it %d BestCost = %.4f\n", iteracao, bestCost);	
							//printf("historico");
							//printCurrentPaths(histPaths, numberOfPairs, numberOfDescriptors);
							//printf("best");
							//printCurrentPaths(bestPaths, numberOfPairs, numberOfDescriptors);
							
						}

						if(currentCost ==bestCost){ //Critério de desempate
							printf("Empate = %f %f\n", bestCost, r->delay);	
							//printCurrentPaths(currentPaths, numberOfPairs, numberOfDescriptors);
							for (int f = 0; f < numberOfPairs*numberOfDescriptors; f++) {
								printf("%d	%.2f	%d\n", f, r->delayFlows[f],arrayLength(arrayGet(currentPaths, f)) );	
							}
							
							if(r->delay < bestDelay){
								
								bestTime = currentTime;
								bestCost = currentCost;
								bestDelay = r->delay;
								for(int p=0; p<numPaths;p++){ //armazena a solução anterior no histórico
									arraySet(histPaths, p, arrayGet(bestPaths, p));
								}

								for(int p=0; p<numPaths;p++){ //obtem a melhor solução
									arraySet(bestPaths, p, arrayGet(currentPaths, p));
								}

								printf("it 0 desempate BestCost = %.4f\n", bestCost);	
								for (int f = 0; f < numberOfPairs*numberOfDescriptors; f++) {
								printf("Flow %lu %.2f - Delay %.2f \n",f, r->rateFlows[f], r->delayFlows[f] );	
								}
	
							}
							


					}

					}
				}
			
				for(i=0; i < numPaths; i++) {
					if(num[i] == 2) {
						num[i] = 0;
						num[i+1]++ ;
					}
				}
			}
    	}
 
		free(num);

		iteracao++;
	}
	

printCurrentPaths(bestPaths, numberOfPairs, numberOfDescriptors);
printf("iteration = %d - bestCost = %f - bestDelay = %f\n", iteracao, bestCost, bestDelay);	



printDSR(bestPaths, numberOfPairs, numberOfDescriptors, rf);
//printSolution(currentPaths, numberOfPairs, numberOfDescriptors, graph);
r = simulationSimulate(graph, bestPaths, simFlowTime, txDurations); //função objetivo
for (int f = 0; f < numberOfPairs*numberOfDescriptors; f++) {
	printf("Flow %lu %.2f - Delay %.2f \n",f, r->rateFlows[f], r->delayFlows[f] );	
}

    arrayFree(currentPaths);
    free(currentPaths);
	
	
    free(r);
	free(rf);
	
	listFreeWithData(dst);
	free(dst);
	listFreeWithData(src);
	free(src);
	listFreeWithData(flt);
	free(flt);
	graphFree(graph);
	free(graph);
	
 	for (i = 0; i < numberOfPairs; i++) {

		pathList = arrayGet(nodePairs, i);
		for (path = listBegin(pathList); path; path = listNext(pathList)) prefixTreePrune(path);
		listFree(pathList);
		free(pathList);
	}

	arrayFree(nodePairs);
	free(nodePairs);

	arrayFree(flowTime);
	free(flowTime);

	arrayFree(simFlowTime);
	free(simFlowTime);

	arrayFree(txDurations);
	free(txDurations);

	arrayFree(currentAuxPaths);
	free(currentAuxPaths);

	arrayFree(neighborPaths);
	free(neighborPaths);

	arrayFree(bestPaths);
	free(bestPaths);

	arrayFree(histPaths);
	free(histPaths);
	
	//fclose(arq);
	
	
	return(0);
}
