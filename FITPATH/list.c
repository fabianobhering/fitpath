#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <string.h>

#include "list.h"
#include "memory.h"

t_list * listNew(void) {

	t_list * newList;

	MALLOC(newList, sizeof(t_list));
	memset(newList, 0, sizeof(t_list));
	newList->atBegining = 1;

	return(newList);
}

int listLength(t_list * list) {

	return(list->num_elements);
}

void * listBegin(t_list * list) {

	list->current = list->first;
	list->atBegining = 0;
	list->currentIndex = 0;

	if (list->current == NULL) return(NULL);

	return(list->current->data);
}

void * listEnd(t_list * list) {

	list->current = list->last;
	list->atBegining = 0;
	list->currentIndex = list->num_elements - 1;

	if (list->current == NULL) return(NULL);

	return(list->current->data);
}

void * listNext(t_list * list) {

	if (list->current != NULL) {

		list->current = list->current->next;
		list->currentIndex++;
	}
	else if (list->atBegining) {

		list->current = list->first;
		list->currentIndex = 0;
		list->atBegining = 0;
	}

	if (list->current == NULL) return(NULL);

	return(list->current->data);
}

void * listPrev(t_list * list) {

	if (list->current != NULL) {

		list->current = list->current->prev;
		list->currentIndex--;

		if (list->current == NULL) list->atBegining = 1;
	}

	if (list->current == NULL) return(NULL);

	return(list->current->data);
}

void * listCurrent(t_list * list) {

	if (list->current == NULL) {
	
		if (list->atBegining) {

			list->current = list->first;
			list->currentIndex = 0;
			list->atBegining = 0;
		}
		else {

			return(NULL);
		}

		if (list->current == NULL) return(NULL);
	}

	return(list->current->data);
}

void listAdd(t_list * list, void * element) {

	t_list_node * newNode;

	MALLOC(newNode, sizeof(t_list_node));
	newNode->data = element;
	newNode->next = NULL;
	newNode->prev = NULL;

	if (list->last == NULL) {

		list->first = newNode;
	}
	else {

		list->last->next = newNode;
		newNode->prev = list->last;
	}

	list->last = newNode;
	list->num_elements++;
}

void listAddAtBegin(t_list * list, void * element) {

	t_list_node * newNode;

	MALLOC(newNode, sizeof(t_list_node));
	newNode->data = element;
	newNode->next = NULL;
	newNode->prev = NULL;

	if (list->first == NULL) {

		list->last = newNode;
	}
	else {

		list->first->prev = newNode;
		newNode->next = list->first;
	}

	list->first = newNode;
	list->num_elements++;
}

void listDelLastWithData(t_list * list) {

	free(list->last->data);
	listDelLast(list);
}

void listDelLast(t_list * list) {

	t_list_node * p;

	p = list->last;
	if (p == NULL) return;

	if (p->prev) 
		p->prev->next = NULL;
	else
		list->first = NULL;

	list->last = p->prev;

	list->num_elements--;
	if (list->current == p) {

		list->current = NULL;
		list->atBegining = 1;
	}
	free(p);
}

void listDel(t_list * list, void * element) {

	t_list_node * p;

	p = list->first;
	if (p == NULL) return;

	if (p->data == element) {

		if (list->last == list->first) list->last = NULL;
		else p->next->prev = p->prev;

		list->first = p->next;
		list->num_elements--;
		if (list->current == p) {

			list->current = NULL;
			list->atBegining = 1;
		}
		free(p);

		return ;
	}

	p = p->next;
	while(p) {

		if (p->data == element) {

			if (list->current == p) {

				list->current = p->prev;
			}
			
			if (list->last == p) list->last = p->prev;
			else p->next->prev = p->prev;

			p->prev->next = p->next;

			free(p);
			list->num_elements--;

			return;
		}

		p = p->next;
	}
}

void listDelCurrent(t_list * list) {

	t_list_node * p;

	if (list->current == NULL) return ;

	if (list->current == list->first) {

		list->first = list->current->next;
		if (list->current == list->last) list->last = NULL;
		else list->current->next->prev = list->current->prev;

		free(list->current);

		list->num_elements--;
		list->current = NULL;
		list->atBegining = 1;

		return ;
	}

	list->current->prev->next = list->current->next;
	if (list->current == list->last) list->last = list->current->prev;
	else list->current->next->prev = list->current->prev;

	p = list->current;
	list->current = list->current->prev;
	free(p);

	list->num_elements--;
}

void listFree(t_list * list) {

	t_list_node * p;

	while(list->first) {

		p = list->first->next;
		free(list->first);
		list->first = p;
	}

	list->last = NULL;
	list->num_elements = 0;
	list->current = NULL;
}

void listFreeWithData(t_list * list) {

	t_list_node * p;

	while(list->first) {

		p = list->first->next;
		free(list->first->data);
		free(list->first);
		list->first = p;
	}

	list->last = NULL;
	list->num_elements = 0;
	list->current = NULL;
}

void listPrint(t_list * list) {

	t_list_node * p;

	p = list->first;
	printf("List has %d elements.\nFirst is in %p and last is in %p.\nCurrent is %p.\n", list->num_elements, 
			list->first, list->last, list->current);
	while(p) {

		printf("In %p:\n\t- Data = \"%s\"\n\n", p, (char *) p->data);
		p = p->next;
	}
}

int listGetIndex(t_list * list) {

	return(list->currentIndex);
}

