#ifndef __PREFIXTREE_H__
#define __PREFIXTREE_H__

#include "array.h"
#include "list.h"
#include "graph.h"

typedef struct t_prefixTreeNode {

	long nodeId;
	unsigned long dummyIndex;
	int hopCount;
	t_weight cost;
	double upperBound;	// Throughput upper bound.
	float simulatedCost;
	int refCount;
	t_array * cachedPath;
	struct t_prefixTreeNode * prefix;
	t_list * sufixes;
	struct t_prefixTreeNode * root;
	unsigned long numberOfNodes;
} t_prefixTreeNode;

typedef struct {

	long nodeId;
	int hopCount;
	t_weight cost;
	double upperBound;	// Throughput upper bound.
	float simulatedCost;
	int refCount;
	unsigned long prefix;
	unsigned long sufixes[0];
} t_persistentPrefixTreeNode;

t_prefixTreeNode * prefixTreeNew(int rootNode);
t_prefixTreeNode * prefixTreeAppend(t_prefixTreeNode * prefix, int node, t_weight lastLinkCost);
void prefixTreePrune(t_prefixTreeNode * prefix);
int prefixTreeGetRefCount(t_prefixTreeNode * prefix);
int prefixTreeGetHopCount(t_prefixTreeNode * prefix);
unsigned long prefixTreeGetNumberOfNodes(t_prefixTreeNode * prefix);
t_weight prefixTreeGetCost(t_prefixTreeNode * prefix);
double prefixTreeGetUpperBound(t_prefixTreeNode * prefix);
int prefixTreeGetNode(t_prefixTreeNode * prefix);
int prefixTreeNodeExists(t_prefixTreeNode * prefix, int node);
void prefixTreePrint(t_prefixTreeNode * prefix);
t_array * prefixTreePath(t_prefixTreeNode * prefix);
t_list * prefixTreeGetSufixes(t_prefixTreeNode * prefix);
t_prefixTreeNode * prefixTreeInsert(t_prefixTreeNode * root, t_array * newPath, t_graph * graph);
t_prefixTreeNode * prefixTreeGetPrefix(t_prefixTreeNode * prefix);
void prefixTreeSetSimulatedCost(t_prefixTreeNode * prefix, float cost);
float prefixTreeGetSimulatedCost(t_prefixTreeNode * prefix);
void prefixTreeSave(t_list * leaves, char * filename);
t_list * prefixTreeLoad(char * filename);

#endif

