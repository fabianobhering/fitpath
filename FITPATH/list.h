#ifndef __LIST_H__
#define __LIST_H__

typedef struct t_list_node {

	void * data;
	struct t_list_node * next;
	struct t_list_node * prev;
} t_list_node;


typedef struct {

	t_list_node * first;
	t_list_node * last;
	t_list_node * current;
	int currentIndex;
	int atBegining;
	int num_elements;
} t_list;

t_list * listNew(void);
int listLength(t_list * list);
void * listBegin(t_list * list);
void * listEnd(t_list * list);
void * listNext(t_list * list);
void * listPrev(t_list * list);
void * listCurrent(t_list * list);
void listAdd(t_list * list, void * element);
void listDel(t_list * list, void * element);
void listDelCurrent(t_list * list);
void listDelLast(t_list * list);
void listDelLastWithData(t_list * list);
void listFree(t_list * list);
void listFreeWithData(t_list * list);
void listPrint(t_list * list);
void listAddAtBegin(t_list * list, void * element);
int listGetIndex(t_list * list);

#endif

