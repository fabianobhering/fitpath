#include "prefixTree.h"
#include "array.h"
#include "graph.h"
#include "memory.h"

t_prefixTreeNode * prefixTreeNew(int rootNode) {

	t_prefixTreeNode * newNode;

	MALLOC(newNode, sizeof(t_prefixTreeNode));

	newNode->nodeId = rootNode;
	newNode->hopCount = 0;
	newNode->refCount = 0;
	newNode->cost = 0;
	newNode->upperBound = INFINITY;
	newNode->cachedPath = NULL;
	newNode->prefix = NULL;
	newNode->sufixes = listNew();
	newNode->root = newNode;
	newNode->numberOfNodes = 1;
	newNode->dummyIndex = 0;

	return(newNode);
}

t_prefixTreeNode * prefixTreeAppend(t_prefixTreeNode * prefix, int node, t_weight lastLinkCost) {

	t_prefixTreeNode * newNode;
	double possibleNewUpperBound;

	MALLOC(newNode, sizeof(t_prefixTreeNode));

	newNode->nodeId = node;
	newNode->hopCount = prefix->hopCount + 1;
	newNode->cost = prefix->cost + lastLinkCost;
	newNode->refCount = 0;
	newNode->cachedPath = NULL;
	newNode->prefix = prefix;
	newNode->sufixes = listNew();
	newNode->root = prefix->root;
	newNode->dummyIndex = newNode->root->numberOfNodes++;
	newNode->numberOfNodes = 0;

	switch(newNode->hopCount) {

		case 1:

			possibleNewUpperBound = 1.0 / newNode->cost;
//printf("possibleNewUpperBound1 = %e\n", possibleNewUpperBound);
			break ;
		case 2:

			possibleNewUpperBound = 1.0 / (newNode->cost - newNode->prefix->prefix->cost);
//printf("possibleNewUpperBound2 = %e\n", possibleNewUpperBound);
			break ;
		default:

			possibleNewUpperBound = 1.0 / (newNode->cost - newNode->prefix->prefix->prefix->cost);
//printf("possibleNewUpperBound3 = %e = %llu - %llu, hops = %d\n", possibleNewUpperBound, newNode->cost, newNode->prefix->prefix->prefix->cost, newNode->hopCount);
			break ;
	}
//printf("possibleNewUpperBound = %e\n", possibleNewUpperBound);
	if (possibleNewUpperBound < newNode->prefix->upperBound) newNode->upperBound = possibleNewUpperBound;
	else newNode->upperBound = newNode->prefix->upperBound;
//printf("newNode->upperBound = %f\n", newNode->upperBound);
	listAdd(prefix->sufixes, newNode);
	prefix->refCount++;

	return(newNode);
}

void prefixTreePrune(t_prefixTreeNode * prefix) {

	t_prefixTreeNode * prev, * next;

	prev = prefix->prefix;
	if (prefix->cachedPath) {

		arrayFree(prefix->cachedPath);
		free(prefix->cachedPath);
	}
	listFree(prefix->sufixes);
	free(prefix->sufixes);
	free(prefix);

	while(prev && prev->refCount == 1) {

		prefix = prev;
		prev = prefix->prefix;
		if (prefix->cachedPath) {

			arrayFree(prefix->cachedPath);
			free(prefix->cachedPath);
		}
		listFree(prefix->sufixes);
		free(prefix->sufixes);
		free(prefix);
	}

	if (prev) {

		for (next = listBegin(prev->sufixes); next != prefix; next = listNext(prev->sufixes));
		listDelCurrent(prev->sufixes);
		prev->refCount--;
	}
}

int prefixTreeGetRefCount(t_prefixTreeNode * prefix) {

	return(prefix->refCount);
}

int prefixTreeGetHopCount(t_prefixTreeNode * prefix) {

	return(prefix->hopCount);
}

t_weight prefixTreeGetCost(t_prefixTreeNode * prefix) {

	return(prefix->cost);
}

double prefixTreeGetUpperBound(t_prefixTreeNode * prefix) {

	return(prefix->upperBound);
}

int prefixTreeGetNode(t_prefixTreeNode * prefix) {

	return(prefix->nodeId);
}

unsigned long prefixTreeGetNumberOfNodes(t_prefixTreeNode * prefix) {

	return(prefix->root->numberOfNodes);
}

int prefixTreeNodeExists(t_prefixTreeNode * prefix, int node) {

	t_prefixTreeNode * p;

	p = prefix;
	while(p) {

		if (p->nodeId == node) return(1);
		p = p->prefix;
	}

	return(0);
}

void prefixTreePrint(t_prefixTreeNode * prefix) {

	t_prefixTreeNode * prev;

	printf("Prefix: %ld ", prefix->nodeId);
	prev = prefix->prefix;
	while(prev) {

		printf("<- %ld ", prev->nodeId);
		prev = prev->prefix;
	}
//	printf("\nUpperBound: %f\n\n", prefix->upperBound);
	printf("\nHops: %d\nCost: " WEIGHT_FORMAT "\nUpperBound: %f\n\n", prefix->hopCount, prefix->cost, prefix->upperBound);
//	printf("\nHops: %d\nCost: " WEIGHT_FORMAT "\n\n", prefix->hopCount, prefix->cost);
}

t_array * prefixTreePath(t_prefixTreeNode * prefix) {

	int i;
	t_prefixTreeNode * p;

	if (!prefix->cachedPath) {

		prefix->cachedPath = arrayNew(prefix->hopCount + 1);
		p = prefix;
		for (i = prefix->hopCount; i >= 0; i--) {

			arraySet(prefix->cachedPath, i, (void *) p->nodeId);
			p = p->prefix;
		}
	}

	return(prefix->cachedPath);
}

t_list * prefixTreeGetSufixes(t_prefixTreeNode * prefix) {

	return(prefix->sufixes);
}

t_prefixTreeNode * prefixTreeInsert(t_prefixTreeNode * root, t_array * newPath, t_graph * graph) {

	t_prefixTreeNode * current, * next;
	int i;
	
	i = 1;
	current = root;
	while(i < arrayLength(newPath)) {

		for (next = listBegin(current->sufixes); next; next = listNext(current->sufixes)) {

			if (next->nodeId == (unsigned long) arrayGet(newPath, i)) {

				i++;
				break ;
			}
		}

		if (next)
			current = next;
		else 
			break ;
	}

	if (i == arrayLength(newPath)) {

		/*
		 * Path already exists in the structure. 
		 * Return NULL to say that.
		 */

		return(NULL);
	}

	for (; i < arrayLength(newPath); i++) {

		t_weight cost = graphGetCost(graph, (unsigned long) arrayGet(newPath, i - 1), (unsigned long) arrayGet(newPath, i));
		if (cost == GRAPH_INFINITY) cost = 1;
		current = prefixTreeAppend(current, (unsigned long) arrayGet(newPath, i), cost);
	}

	return(current);
}

t_prefixTreeNode * prefixTreeGetPrefix(t_prefixTreeNode * prefix) {

	return(prefix->prefix);
}

void prefixTreeSetSimulatedCost(t_prefixTreeNode * prefix, float cost) {

	prefix->simulatedCost = cost;
}

float prefixTreeGetSimulatedCost(t_prefixTreeNode * prefix) {

	return(prefix->simulatedCost);
}

#ifdef OLD
void prefixTreeSave2(t_list * leaves, char * filename) {

	t_prefixTreeNode * p1, * p2, * sufix;
	t_persistentPrefixTreeNode persistentNode;
	t_list * ptr2index;
	FILE * outputFile;
	unsigned long i;

	ptr2index = listNew();
	outputFile = fopen(filename, "w");
	if (!outputFile) {

		fprintf(stderr, "Failed to create file '%s'\n", filename);
		exit(1);
	}

	/*
	 * First, we include all the leafs so that the order is kept.
	 * Notice that these nodes are never prefixes from other nodes,
	 * so we don't need to add them to the ptr2index translation 
	 * list (only their parents).
	 */
	for (p1 = listBegin(leaves); p1; p1 = listNext(leaves)) {

		persistentNode.nodeId = p1->nodeId;
		persistentNode.hopCount = p1->hopCount;
		persistentNode.cost = p1->cost;
		persistentNode.upperBound = p1->upperBound;
		persistentNode.simulatedCost = p1->simulatedCost;
		persistentNode.refCount = p1->refCount;

		/*
		 * Look for the prefix and add it to the list if we did not
		 */
		for (p2 = listBegin(ptr2index); p2; p2 = listNext(ptr2index)) {

			if (p2 == p1->prefix) break ;
		}

		if (p2) {

			/*
			 * Node already exists in the translation list.
			 */
			
			persistentNode.prefix = listLength(leaves) + listGetIndex(ptr2index);
		}
		else {

			persistentNode.prefix = listLength(leaves) + listLength(ptr2index);
			listAdd(ptr2index, p1->prefix);
		}

		fwrite(& persistentNode, 1, sizeof(persistentNode), outputFile);
	}

	/*
	 * Now, we go through the translation list. The nodes there are those 
	 * which we did not output to the file. Notice that new nodes can be
	 * appended to the (end of the) list as we go. In the end, we'll have
	 * covered everything.
	 */
	for (p1 = listBegin(ptr2index); p1; p1 = listNext(leaves)) {

		/*
		 * Look for the node in the conversion table. The node 
		 * must be there already.
		 */
		persistentNode.nodeId = p1->nodeId;
		persistentNode.hopCount = p1->hopCount;
		persistentNode.cost = p1->cost;
		persistentNode.upperBound = p1->upperBound;
		persistentNode.simulatedCost = p1->simulatedCost;
		persistentNode.refCount = p1->refCount;

		/*
		 * Look for the prefix and add it to the list if we do not
		 * find it.
		 */
		for (p2 = listBegin(ptr2index); p2; p2 = listNext(ptr2index)) {

			if (p2 == p1->prefix) break ;
		}

		if (p2) {

			/*
			 * Node already exists in the translation list.
			 */
			
			persistentNode.prefix = listLength(leaves) + listGetIndex(ptr2index);
		}
		else {

			persistentNode.prefix = listLength(leaves) + listLength(ptr2index);
			listAdd(ptr2index, p1->prefix);
		}

		fwrite(& persistentNode, 1, sizeof(persistentNode), outputFile);

		for (sufix = listBegin(p1->sufixes); sufix; sufix = listNext(p1->sufixes)) {

			/*
			 * Look for the sufixes and add it to the list if we did not
			 */
			for (p2 = listBegin(ptr2index); p2; p2 = listNext(ptr2index)) {

				if (p2 == sufix) break ;
			}

			if (p2) {

				/*
				 * Node already exists in the translation list.
				 */
				i = listGetIndex(ptr2index) + listLength(leaves);
				fwrite(& i, 1, sizeof(unsigned long), outputFile);
			}
			else {

				/*
				 * Look for in the leaves.
				 */
				for (p2 = listBegin(leaves); p2; p2 = listNext(leaves)) {

					if (p2 == p1->prefix) break ;
				}

				if (p2) {
				
					i = listGetIndex(leaves);
					fwrite(& i, 1, sizeof(unsigned long), outputFile);
				}
				else {

					i = listLength(leaves) + listLength(ptr2index);
					fwrite(& i, 1, sizeof(unsigned long), outputFile);
					listAdd(ptr2index, sufix);
				}
			}
		}

		/*
		 * At this point, the internal pointer of list ptr2index is 
		 * lost (because we started over the iteration in the middle 
		 * of the for). We have to put it back to the previous point.
		 */
		for (p2 = listBegin(ptr2index); p2 != p1; p2 = listNext(ptr2index));
	}

	fclose(outputFile);
}
#endif

void prefixTreeSave(t_list * leaves, char * filename) {

	t_prefixTreeNode * p1, * sufix;
	t_persistentPrefixTreeNode persistentNode;
	t_array * ptr2index, * index2ptr;
	FILE * outputFile;
	unsigned long i, j, outputIndex, numberOfNodes;
	int res;

	numberOfNodes = prefixTreeGetNumberOfNodes(listBegin(leaves));
//printf("Storing number of nodes as %lu\n", numberOfNodes);
	ptr2index = arrayNew(numberOfNodes);
	index2ptr = arrayNew(numberOfNodes);
	arrayClear(ptr2index);
	arrayClear(index2ptr);

	outputFile = fopen(filename, "w");
	if (!outputFile) {

		fprintf(stderr, "Failed to create file '%s'\n", filename);
		exit(1);
	}

	res = fwrite(& numberOfNodes, 1, sizeof(numberOfNodes), outputFile);
	if (res < 0) {

		fprintf(stderr, "Error writing to file...\n");
		exit(1);
	}

	/*
	 * First, we include all the leafs so that the order is kept.
	 */
	i = 0;
	for (p1 = listBegin(leaves); p1; p1 = listNext(leaves)) {

		persistentNode.nodeId = p1->nodeId;
		persistentNode.hopCount = p1->hopCount;
		persistentNode.cost = p1->cost;
		persistentNode.upperBound = p1->upperBound;
		persistentNode.simulatedCost = p1->simulatedCost;
		persistentNode.refCount = p1->refCount;

		arraySet(ptr2index, p1->dummyIndex, (void *) ((unsigned long) listGetIndex(leaves) + 1));
		arraySet(index2ptr, listGetIndex(leaves), p1->prefix);

		/*
		 * Look for the prefix and add it to the list if we did not
		 */
		if (!arrayGet(ptr2index, p1->prefix->dummyIndex)) {

			arraySet(ptr2index, p1->prefix->dummyIndex, (void *) (i + listLength(leaves) + 1));
			arraySet(index2ptr, i + listLength(leaves), p1->prefix);
			i++;
		}
		persistentNode.prefix = ((unsigned long) arrayGet(ptr2index, p1->prefix->dummyIndex)) - 1;
//printf("Storing next node..\n");
		res = fwrite(& persistentNode, 1, sizeof(persistentNode), outputFile);
		if (res < 0) {

			fprintf(stderr, "Error writing to file...\n");
			exit(1);
		}
	}

	/*
	 * Now, we go through the translation table. The nodes there are those 
	 * which we did not output to the file. Notice that new nodes can be
	 * appended to the table as we go. In the end, we'll have
	 * covered everything.
	 */
	for (j = listLength(leaves); j < arrayLength(index2ptr); j++) {

		p1 = arrayGet(index2ptr, j);
		if (p1 == NULL) break ;

		persistentNode.nodeId = p1->nodeId;
		persistentNode.hopCount = p1->hopCount;
		persistentNode.cost = p1->cost;
		persistentNode.upperBound = p1->upperBound;
		persistentNode.simulatedCost = p1->simulatedCost;
		persistentNode.refCount = p1->refCount;

		/*
		 * Look for the prefix and add it to the list if we do not
		 * find it.
		 */
		if (p1->prefix) {

			if (!arrayGet(ptr2index, p1->prefix->dummyIndex)) {

				arraySet(ptr2index, p1->prefix->dummyIndex, (void *) (i + listLength(leaves) + 1));
				arraySet(index2ptr, i + listLength(leaves), p1->prefix);
				i++;
			}
			persistentNode.prefix = ((unsigned long) arrayGet(ptr2index, p1->prefix->dummyIndex)) - 1;
		}
		else {

			persistentNode.prefix = j;
		}

//printf("Storing next node..\n");
		res = fwrite(& persistentNode, 1, sizeof(persistentNode), outputFile);
		if (res < 0) {

			fprintf(stderr, "Error writing to file...\n");
			exit(1);
		}

//printf("P1 has ref count -of %lu..\n", p1->refCount);
		/*
		 * Handle the sufixes.
		 */
		for (sufix = listBegin(p1->sufixes); sufix; sufix = listNext(p1->sufixes)) {

			/*
			 * Look for the prefix and add it to the list if we do not
			 * find it.
			 */
			if (!arrayGet(ptr2index, sufix->dummyIndex)) {

				arraySet(ptr2index, sufix->dummyIndex, (void *) (i + listLength(leaves) + 1));
				arraySet(index2ptr, i + listLength(leaves), sufix);
				i++;
			}
//printf("Storing sufix reference\n");
			outputIndex = ((unsigned long) arrayGet(ptr2index, sufix->dummyIndex)) - 1;
			res = fwrite(& outputIndex, 1, sizeof(unsigned long), outputFile);
			if (res < 0) {

				fprintf(stderr, "Error writing to file...\n");
				exit(1);
			}
		}
	}

	fclose(outputFile);

	arrayFree(ptr2index);
	arrayFree(index2ptr);
	free(ptr2index);
	free(index2ptr);
}

t_list * prefixTreeLoad(char * filename) {

	t_prefixTreeNode * newNode;
	t_persistentPrefixTreeNode persistentNode;
	t_array * index2ptr;
	t_list * tmp;
	FILE * inputFile;
	unsigned long nodesRead, inputIndex, numberOfNodes, i;
	int res;
	t_list * paths;

	inputFile = fopen(filename, "r");
	if (!inputFile) {

		fprintf(stderr, "Failed to open file '%s'\n", filename);
		exit(1);
	}

	res = fread(& numberOfNodes, sizeof(numberOfNodes), 1, inputFile);
	if (res <= 0) {

		fprintf(stderr, "Error reading file...\n");
		exit(1);
	}

	index2ptr = arrayNew(numberOfNodes);
	arrayClear(index2ptr);
	paths = listNew();
//printf("Read %lu nodes\n", numberOfNodes);
	/*
	 * First, we read the file and organize nodes in an
	 * array.
	 */
	nodesRead = 0;
	while(!feof(inputFile)) {
//printf("File is at %li\n", ftell(inputFile));
		res = fread(& persistentNode, sizeof(persistentNode), 1, inputFile);
		if (res <= 0) {

			if (feof(inputFile)) break ;
			fprintf(stderr, "Error reading file...\n");
			exit(1);
		}

		MALLOC(newNode, sizeof(t_prefixTreeNode));

//printf("Storing node of index %lu (res = %d)\n", nodesRead, res);
//fflush(stdout);
		newNode->nodeId = persistentNode.nodeId;
		newNode->hopCount = persistentNode.hopCount;
		newNode->cost = persistentNode.cost;
		newNode->simulatedCost = persistentNode.simulatedCost;
		newNode->upperBound = persistentNode.upperBound;
		newNode->refCount = persistentNode.refCount;
		newNode->cachedPath = NULL;
		newNode->prefix = (void *) persistentNode.prefix;
		newNode->sufixes = listNew();
//printf("Node has refcounf of %lu\n", newNode->refCount);
		for (i = 0; i < newNode->refCount; i++) {

//printf("File is at %li\n", ftell(inputFile));
			res = fread(& inputIndex, sizeof(inputIndex), 1, inputFile);
			if (res <= 0) {

				fprintf(stderr, "Error reading file...\n");
				exit(1);
			}
//printf("Reading sufix...\n");	
//fflush(stdout);
			/*
			 * Avoid index = 0.
			 */
			inputIndex++;
			listAdd(newNode->sufixes, (void *) inputIndex);
		}
		arraySet(index2ptr, nodesRead++, newNode);

		if (newNode->refCount == 0) listAdd(paths, newNode);
	}

	fclose(inputFile);

	/*
	 * Now, go through the recently created node array and fix
	 * the pointers.
	 */
	
	for (i = 0; i < nodesRead; i++) {
	
		newNode = arrayGet(index2ptr, i);
		if ((unsigned long) newNode->prefix == i) newNode->prefix = NULL;
		else newNode->prefix = arrayGet(index2ptr, (unsigned long) newNode->prefix);

		tmp = listNew();
		for (inputIndex = (unsigned long) listBegin(newNode->sufixes); inputIndex; inputIndex = (unsigned long) listNext(newNode->sufixes)) {

			listAdd(tmp, arrayGet(index2ptr, inputIndex - 1));
		}

		listFree(newNode->sufixes);
		free(newNode->sufixes);
		newNode->sufixes = tmp;
	}

	arrayFree(index2ptr);
	free(index2ptr);

	return(paths);
}




