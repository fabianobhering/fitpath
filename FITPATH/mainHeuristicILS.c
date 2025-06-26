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
void printDSR(t_array * paths[], int numberOfPairs, int numberOfDescriptors, t_return * rf ){  
    printf("DSR Routes\n");
    //INST++;
    FILE *arq_ns3, *arq_result;
    char arq_name[25];
	sprintf(arq_name,"../inst/routes/3s-route300_60_ref%d", REF);
  
    arq_ns3 = fopen(arq_name, "a+");
    
	 
		fprintf(arq_ns3,"if(INST == %d){\n",INST);	
		for (int i = 0; i < numberOfPairs*numberOfDescriptors; i++) { // Unir os caminhos para simular 
			fprintf(arq_ns3,"if(flowId == %ld){",10+i+1);	
			for (int j = arrayLength(arrayGet(paths, i))-1; j >=0 ; j--) {
				fprintf(arq_ns3,"	pathILS.push_back (Ipv4Address (\"10.0.0.%d\"));", (unsigned long) arrayGet(arrayGet(paths, i), j)+1);
			}
			
			fprintf(arq_ns3,"}\n");
		
		}    
		
		for (int i = 0; i < numberOfPairs*numberOfDescriptors; i++) { // Unir os caminhos para simular 
			fprintf(arq_ns3,"if(flowId == %ld){",i+1);	
			for (int j = 0; j < arrayLength(arrayGet(paths, i)); j++) {
				fprintf(arq_ns3,"	pathILS.push_back (Ipv4Address (\"10.0.0.%d\"));", (unsigned long) arrayGet(arrayGet(paths, i), j)+1);
			}
			fprintf(arq_ns3,"}\n");
		
		}    
		fprintf(arq_ns3,"}\n");
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
	
	int numberOfDescriptors = 2; //quantidade de descritores
	int numberOfPathsPerFlow = 10; // S = conjunto de soluções para cada fluxo
   
	t_graph * graph;
	t_list * pathList;
	t_list * src, * dst, ** flt;
	t_prefixTreeNode * path;
	t_array * nodePairs, * flowTime, * simFlowTime, * txDurations;
	t_array * currentPaths,	* currentPathsILS[numberOfDescriptors], * currentPathsAuxILS[numberOfDescriptors], * currentPathsPerturbaILS[numberOfDescriptors], * bestPathsILS[numberOfDescriptors], * histPathsILS[numberOfDescriptors];
	float  bestCost, currentCost, pertCost, bestTime, currentTime;
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
    
	REF = atoi(argv[2]);
	INST = atoi(argv[3]);

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
		
		currentSrc = listCurrent(src); // lista de fonte do arquivo
		//* randSrc = rand()%numberOfNodes;
		//currentSrc = randSrc; //Fonte Aleatória
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
		printf("flow %d - %d\n", f, * currentFlt);
		f++;		
		if (listNext(flt) == NULL) break ;
	}
	


/*
//********* Avaliar um caminho específico *****************
	//Novos valores para os fluxos
	arraySet(flowTime, 0,1757); 
	arraySet(flowTime, 1,1757); 
	arraySet(flowTime, 2,1757); 
	arraySet(flowTime, 3,1757); 

	currentPaths = arrayNew(4);
	t_array * pathEval;
	pathEval = arrayNew(6);
	arraySet(pathEval, 0, 1);
	arraySet(pathEval, 1, 10);
	arraySet(pathEval, 2, 34);
	arraySet(pathEval, 3, 20);
	arraySet(pathEval, 4, 11);
	arraySet(pathEval, 5, 0);

	arraySet(currentPaths, 0, pathEval);

	pathEval = arrayNew(6);
	arraySet(pathEval, 0, 1);
	arraySet(pathEval, 1, 10);
	arraySet(pathEval, 2, 34);
	arraySet(pathEval, 3, 20);
	arraySet(pathEval, 4, 11);
	arraySet(pathEval, 5, 0);
	arraySet(currentPaths, 1, pathEval);

	pathEval = arrayNew(5);
	arraySet(pathEval, 0, 2);
	arraySet(pathEval, 1, 7);
	arraySet(pathEval, 2, 16);
	arraySet(pathEval, 3, 33);
	arraySet(pathEval, 4, 0);
	arraySet(currentPaths, 2, pathEval);

	pathEval = arrayNew(5);
	arraySet(pathEval, 0, 2);
	arraySet(pathEval, 1, 7);
	arraySet(pathEval, 2, 16);
	arraySet(pathEval, 3, 33);
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
    bestCost = r->meanInterval;
    currentTime = ((float)clock() - t)/((CLOCKS_PER_SEC/1000));
    bestTime = currentTime;
    rf=r; // melhor fluxo retornado
    printf(" EvalPath Cost = %.2f\n", bestCost);
return(0);
*/
//**************************************************************


	
	
	
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
	simFlowTime = arrayNew(numberOfPairs*numberOfDescriptors); 
	
	for (int d = 0; d < numberOfDescriptors; d++) {
		bestPathsILS[d] = arrayNew(numberOfPairs); 
		histPathsILS[d] = arrayNew(numberOfPairs); 
		currentPathsILS[d] = arrayNew(numberOfPairs); 
		currentPathsAuxILS[d] = arrayNew(numberOfPairs);
		currentPathsPerturbaILS[d] = arrayNew(numberOfPairs);
	}
	  
    t = clock(); //armazena tempo
    
	for (i = 0; i < numberOfPairs; i++) { // atribui a solução inicial em s0
		
        path = listBegin(arrayGet(nodePairs, i)); // escolhe o caminho mais curto - primeiro
		arraySet(currentPathsILS[0], i, prefixTreePath(path));
		arraySet(currentPathsAuxILS[0], i, prefixTreePath(path));
		arraySet(currentPathsPerturbaILS[0], i, prefixTreePath(path));
		arraySet(bestPathsILS[0], i, prefixTreePath(path));
		arraySet(histPathsILS[0], i, prefixTreePath(path));
		
		for (int d = 1; d < numberOfDescriptors; d++) {
			//path = listNext(arrayGet(nodePairs, i));
			arraySet(currentPathsILS[d], i, prefixTreePath(path));
			arraySet(currentPathsAuxILS[d], i, prefixTreePath(path));
			arraySet(currentPathsPerturbaILS[d], i, prefixTreePath(path));
			arraySet(bestPathsILS[d], i, prefixTreePath(path));
			arraySet(histPathsILS[d], i, prefixTreePath(path));
			//printf("path len: %d\n",listGetIndex(arrayGet(nodePairs, i)));  
     	 }
	}
	numberOfPathsPerFlow=numberOfPathsPerFlow-2;
	//printf("%d\n",numberOfPathsPerFlow);  
    
    
    printf("S0\n");
	c = 0;
	for (i = 0; i < numberOfPairs; i++) {
		for (int d = 0; d < numberOfDescriptors; d++) {
			for (j = 0; j < arrayLength(arrayGet(bestPathsILS[d], i)); j++) {
				printf("%lu ", (unsigned long) arrayGet(arrayGet(bestPathsILS[d], i), j));
			}
			printf(" - FLT %d\n", arrayGet(flowTime, c));
			c++;
		}
		
    }
    
    c = 0;
    for (i = 0; i < numberOfPairs; i++) { // Unir os caminhos para simular 
       for (int d = 0; d < numberOfDescriptors; d++) {
			arraySet(currentPaths, c, arrayGet(currentPathsILS[d], i));
			arraySet(simFlowTime, c, arrayGet(flowTime, c));
			c++;
	   } 
    }
    printf("currentPaths\n");
    for (i = 0; i < numberOfPairs*numberOfDescriptors; i++) { // Unir os caminhos para simular 
   		for (j = 0; j < arrayLength(arrayGet(currentPaths, i)); j++) {
            printf("%lu ",(unsigned long) arrayGet(arrayGet(currentPaths, i), j));
     }
     printf(" - FLT %d \n", arrayGet(simFlowTime, i));
    }    
    
	MALLOC(r, sizeof(t_return));
	MALLOC(rf, sizeof(t_return));
    r = simulationSimulate(graph, currentPaths, simFlowTime, txDurations); //função objetivo
    bestCost = r->cost;
    currentTime = ((float)clock() - t)/((CLOCKS_PER_SEC/1000));
    bestTime = currentTime;
    rf=r; // melhor fluxo retornado
    printf("S0 Cost = %.4f\n", bestCost);
    
	return 0;
/*
	FILE *arq_delay;
    char arq_name[25];
	sprintf(arq_name,"../inst/routes/newMATE/delay.txt");
	arq_delay = fopen(arq_name, "a+");
	for (int f = 0; f < numberOfPairs*numberOfDescriptors; f++) {
		//printf("Flow %lu %.2f - Delay %.2f \n",f, r->meanIntervalPerFlow[f], r->delayFlows[f] );	
		printf("%d	%d	%.2f \n",INST, f, r->delayFlows[f] );	
		fprintf(arq_delay,"%d	%d	%.2f \n",INST, f, r->delayFlows[f] );	
	}
	*/
	//printDSR(currentPaths, numberOfPairs, numberOfDescriptors, rf);
	//return(0);
    
    
    
    
	//char arq_name[10];
	//sprintf(arq_name,"F2_%d_%d",graphSize(graph),listLength(src));
    //arq = fopen(arq_name, "a+");
	//fprintf(arq,"%lu	", (unsigned long)bestCost);
	//fprintf(arq,"------------------\n");
	//fprintf(arq,"*	%d	%lu	%lu\n", numberOfDescriptors, (unsigned long)bestCost, (unsigned long)bestTime);
	////// Disjunto
	// fclose(arq);
	// return(0);
	/////
	 	
	/* Busca Local - Permutação */
    /*
	printf("Busca Local\n");
	
    int *num;
    int n = numberOfDescriptors;
    int rb =numberOfPairs;
    num = (int *)calloc(rb+1, sizeof(int)) ;
    if ( num == NULL ) {
        perror("malloc") ;
        return -1;
    }
    
    while ( num[rb] == 0 ) {
        for(i=0; i < n; i++) {
            
            for(j=0; j < rb; j++) {
                printf("%d-%d ",j, num[j]);
                //implementar a avaliação aqui.
            }
            putchar('\n') ;
			num[0]++ ;
        }
        
        for(i=0; i < rb; i++) {
            if(num[i] == n) {
                num[i] = 0;
                num[i+1]++ ;
            }
        }
    }
 
    free(num) ;
	*/
	c = 0;
	for (i = 0; i < numberOfPairs; i++) { // BuscaLocal(s0) - para cada Fonte F
        
		if (i>0){
					//printf("flow %d path %d\n", i, p);
					path = listBegin(arrayGet(nodePairs, i-1)); // volta com primeiro caminho
					arraySet(currentPathsAuxILS[0], i-1, prefixTreePath(path));
			    }

        path = listNext(arrayGet(nodePairs, i)); // newPath(i); Conferir se precisa. 
		//printf("path len: %d\n",listGetIndex(arrayGet(nodePairs, i)));  
        if (path == NULL) {
			if (i == numberOfPairs - 1) {
				break ;
            }
        } 
        
        for(int d=0; d<numberOfDescriptors;d++){
			/*
			printf(" trocou \n");
			for (j = 0; j < arrayLength(arrayGet(currentPathsAuxILS[d], i)); j++) {	
				printf("%lu ", (unsigned long) arrayGet(arrayGet(currentPathsAuxILS[d], i), j));
			}
			*/
			arraySet(currentPathsAuxILS[d], i, prefixTreePath(path));
			/*
			printf("\n por\n");
			for (j = 0; j < arrayLength(arrayGet(currentPathsAuxILS[d], i)); j++) {	
				printf("%lu ", (unsigned long) arrayGet(arrayGet(currentPathsAuxILS[d], i), j));
			}
			printf("\n");
			*/
			arraySet(currentPaths, c, arrayGet(currentPathsAuxILS[d], i));
			
			
			r = simulationSimulate(graph, currentPaths, simFlowTime, txDurations); //função objetivo
			currentCost = r->cost;
			currentTime = ((float)clock() - t)/((CLOCKS_PER_SEC/1000));
			if (currentCost <bestCost) {
				bestTime = currentTime;
				bestCost = currentCost;
				arraySet(currentPathsILS[d], i, arrayGet(currentPathsAuxILS[d], i));
				arraySet(histPathsILS[d], i, arrayGet(bestPathsILS[d], i));
				arraySet(bestPathsILS[d], i, arrayGet(currentPathsAuxILS[d], i));
				printf(" Melhorou S0 \n");
				c = 0;
				for (i = 0; i < numberOfPairs; i++) {
					for (int d = 0; d < numberOfDescriptors; d++) {
						for (j = 0; j < arrayLength(arrayGet(bestPathsILS[d], i)); j++) {
							printf("%lu ", (unsigned long) arrayGet(arrayGet(bestPathsILS[d], i), j));
						}
						printf(" - FLT %d\n", arrayGet(flowTime, c));
						c++;
					}
					
				}
				printf(" BestCost = %.4f\n", bestCost);
				for (int f = 0; f < numberOfPairs*numberOfDescriptors; f++) {
						printf("Flow %lu %.2f - Delay %.2f \n",f, r->meanIntervalPerFlow[f], r->delayFlows[f] );
		
				}
				printf("Tempo de execucao: %lf \n", currentTime); //conversão para double
			}else{
				arraySet(currentPaths, c, arrayGet(currentPathsILS[d], i));
			}
			c++;
		}
		 
		
                    
	}
	numberOfPathsPerFlow--;
	//printf("%d\n",numberOfPathsPerFlow);  
	/*
	for (int d = 0; d < numberOfDescriptors; d++) {
		for (i = 0; i < arrayLength(bestPathsILS[d]); i++) {
			for (j = 0; j < arrayLength(arrayGet(bestPathsILS[d], i)); j++) {
				printf("%lu ", (unsigned long) arrayGet(arrayGet(bestPathsILS[d], i), j));
			}
			printf("\n");
		}
    }
    */
	//fprintf(arq,"**	%lu	%lu\n",(unsigned long)bestCost, ((unsigned long)bestTime)); 
    
    int tentativas=0, iteracao =1, bestIt=0;
    while (bestCost>0.00001 & numberOfPathsPerFlow>1 & currentTime<100000){ //Reduzir o time para instancias que não atingirem o tempo
		 
        /* s' = Perturbacao (s, historico);
           s''= BuscaLocal(s');
           s = CriterioAceitacao(s,s'',historico)
         */
        
       //if (((double)clock() - t)/((CLOCKS_PER_SEC/1000)) > 100000.00) break;		
       
        /*
         Perturbação
         
         para cada fluxo F
         j = rand(0, length(p)
         s[i][j] = historico [i][j]
         */
        for (i = 0; i < numberOfPairs; i++) { //realiza a perturbação
			for(int d=0;d<numberOfDescriptors;d++){
				 arraySet(currentPathsPerturbaILS[d], i, arrayGet(currentPathsILS[d], i));
			}
            
            int randPerturb = rand()%numberOfDescriptors;
            /*
            printf("perturbação path %d descriptor %d\n",i,randPerturb);
            for (j = 0; j < arrayLength(arrayGet(currentPathsPerturbaILS[randPerturb], i)); j++) {
				  printf("%lu ", (unsigned long) arrayGet(arrayGet(currentPathsPerturbaILS[randPerturb], i), j));   
            }
            printf("\n");
            for (j = 0; j < arrayLength(arrayGet(histPathsILS[randPerturb], i)); j++) {
				  printf("%lu ", (unsigned long) arrayGet(arrayGet(histPathsILS[randPerturb], i), j));
            }
            printf("\n");
            */
            arraySet(currentPathsPerturbaILS[randPerturb], i, arrayGet(histPathsILS[randPerturb], i));   
        }
        
        int c = 0;
		for (i = 0; i < numberOfPairs; i++) { // Unir os caminhos para simular 
			for (int d = 0; d < numberOfDescriptors; d++) {
				arraySet(currentPaths, c, arrayGet(currentPathsILS[d], i));
				c++;
			}
		}
        r = simulationSimulate(graph, currentPaths, simFlowTime, txDurations); //função objetivo
		pertCost = r->cost;
        //printf("pertCost = %.2f\n", pertCost);
       
        //BUSCA LOCAL
        c=0;
        for (i = 0; i < numberOfPairs; i++) { // BuscaLocal(s0) - para cada Fluxo F
       
			path = listNext(arrayGet(nodePairs, i)); // newPath(i);
				//printf("path len: %d\n",listGetIndex(arrayGet(nodePairs, i)));  
				if (path == NULL) {         
					if (i == numberOfPairs - 1) {
						break ;
					}
				}
				
			for(int d=0; d<numberOfDescriptors;d++){
				/*
				printf(" trocou \n");
				for (j = 0; j < arrayLength(arrayGet(currentPathsAuxILS[d], i)); j++) {
					printf("%lu ", (unsigned long) arrayGet(arrayGet(currentPathsAuxILS[d], i), j));
				}
				*/
				
				arraySet(currentPathsAuxILS[d], i, prefixTreePath(path));
				
				/*
				printf("\n por\n");
				for (j = 0; j < arrayLength(arrayGet(currentPathsAuxILS[d], i)); j++) {
					printf("%lu ", (unsigned long) arrayGet(arrayGet(currentPathsAuxILS[d], i), j));
				}
				*/
				arraySet(currentPaths, c, arrayGet(currentPathsAuxILS[d], i));
				
				
				r = simulationSimulate(graph, currentPaths, simFlowTime, txDurations); //função objetivo
				currentCost = r->cost;
				currentTime = ((float)clock() - t)/((CLOCKS_PER_SEC/1000));
				//printf("\n trocou %d %d - %.2f\n", i,d,currentCost );
				
				if (currentCost <bestCost) {
					bestIt = iteracao;
					bestTime = currentTime;
					bestCost = currentCost;
					arraySet(currentPathsILS[d], i, arrayGet(currentPathsAuxILS[d], i));
					arraySet(histPathsILS[d], i, arrayGet(bestPathsILS[d], i));
					arraySet(bestPathsILS[d], i, arrayGet(currentPathsAuxILS[d], i));
					printf(" Melhorou iteração %d  \n",iteracao);
					printf(" BestCost = %.4f\n", bestCost);
					for (int f = 0; f < numberOfPairs*numberOfDescriptors; f++) {
						printf("Flow %lu %.2f - Delay %.2f \n",f, r->meanIntervalPerFlow[f], r->delayFlows[f] );
						
		
					}
					printf("Tempo de execucao: %lf \n", currentTime); //conversão para double
					rf=r;
					c = 0;
					for (i = 0; i < numberOfPairs; i++) {
						for (int d = 0; d < numberOfDescriptors; d++) {
							for (j = 0; j < arrayLength(arrayGet(bestPathsILS[d], i)); j++) {
								printf("%lu ", (unsigned long) arrayGet(arrayGet(bestPathsILS[d], i), j));
							}
							printf(" - FLT %d\n", arrayGet(flowTime, c));
							c++;
						}
						
					}

					//fprintf(arq,"%d	%lu	%lu\n",iteracao, (unsigned long)bestCost, ((unsigned long)bestTime));
					//fprintf(arq,"*");
					//for (i = 0; i < arrayLength(rf->flow); i++) {
					//	printf("Fluxo   %d: %d  %d  %d  \n", i, (int) arrayGet(rf->flow,i), (int) arrayGet(rf->time,i), (int) arrayGet(rf->delivery,i));
					//
					tentativas=0;
				}else{
					if(currentCost==bestCost) tentativas++; 
					arraySet(currentPaths, c, arrayGet(currentPathsILS[d], i));
					
				}
				c++;
				
			}
			
			
		}
		currentTime = ((float)clock() - t)/((CLOCKS_PER_SEC/1000));
		numberOfPathsPerFlow--;
		//fprintf(arq, "time	%lu\n", (unsigned long)currentTime); //conversão para double
		//printf("%d\n",numberOfPathsPerFlow);  
		//fprintf(arq,"%d	%lu	%lu\n",iteracao, (unsigned long)bestCost, ((unsigned long)bestTime)); 
		tentativas++;
        iteracao++;  
        
	}
	//fprintf(arq,"**	%d	%lu	%lu	%d\n",numberOfDescriptors, (unsigned long)bestCost, ((unsigned long)bestTime), bestIt); 
		
	printf("bestPaths\n");
	for (i = 0; i < numberOfPairs; i++) {
		for (int d = 0; d < numberOfDescriptors; d++) {
			for (j = 0; j < arrayLength(arrayGet(bestPathsILS[d], i)); j++) {
				printf("%lu ", (unsigned long) arrayGet(arrayGet(bestPathsILS[d], i), j));
			}
			printf("\n");
		}
    }
    
	
	printDSR(currentPaths, numberOfPairs, numberOfDescriptors, rf);

    //fprintf(arq,"max	%lu     %lu\n", (unsigned long)bestCost, ((unsigned long)bestTime)); 
 	//fprintf(arq,"Best path set has cost %.2f and is:\n", bestCost);
    /* Imprimir Custo por Fluxo
    fprintf(arq,"Fluxo	Custo	Vazão\n");
	for (i = 0; i < arrayLength(bestPaths1); i++) {
        
		for (j = 0; j < arrayLength(arrayGet(bestPaths1, i)); j++) {
			
			fprintf(arq,"%lu ", (unsigned long) arrayGet(arrayGet(bestPaths1, i), j));
			
			
            
		}
        fprintf(arq, "	%lu	%lu\n",(unsigned long) arrayGet(rf->flow, i), (unsigned long)(CLOCKS_PER_SEC/(unsigned long) arrayGet(rf->flow, i)));
        
        for (j = 0; j < arrayLength(arrayGet(bestPaths2, i)); j++) {
            
            fprintf(arq,"%lu ",(unsigned long) arrayGet(arrayGet(bestPaths2, i), j));
        }
		fprintf(arq, "	%lu	%lu\n",(unsigned long) arrayGet(rf->flow, i+1), (unsigned long)(CLOCKS_PER_SEC/(unsigned long) arrayGet(rf->flow, i+1)));
		
	}
	
    
	arrayFree(bestPathsILS[numberOfDescriptors]);
	free(bestPathsILS[numberOfDescriptors]);    
    
    arrayFree(histPathsILS[numberOfDescriptors]);
    free(histPathsILS[numberOfDescriptors]);
    
    //arrayFree(currentPathsILS[numberOfDescriptors]);
    free(currentPathsILS[numberOfDescriptors]);
    
    arrayFree(currentPathsAuxILS[numberOfDescriptors]);
    free(currentPathsAuxILS[numberOfDescriptors]);
   
    arrayFree(currentPathsPerturbaILS[numberOfDescriptors]);
    free(currentPathsPerturbaILS[numberOfDescriptors]);
    arrayFree(currentPaths);
    free(currentPaths);
	
	
    
	
	listFreeWithData(dst);
	free(dst);
	listFreeWithData(src);
	free(src);
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
	* */
	//fclose(arq);
	
	
	return(0);
}
