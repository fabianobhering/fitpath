#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "parserOnlyPaths.h"
#include "graph.h"
#include "list.h"
#include "memory.h"
#include "array.h"

typedef enum {

	TOKEN_LINKS_KEYWORD,
	TOKEN_PATHS_KEYWORD,
	TOKEN_END_PATH,
	TOKEN_INTEGER,
	TOKEN_REAL,
	TOKEN_EOF,
	TOKEN_ERROR,
	NUMBER_OF_TOKENS,
} t_token;

#define IS_DIGIT(ch) (ch >= '0' && ch <= '9')
#define IS_DOT(ch) (ch == '.')
#define IS_LETTER(ch) ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'))
#define IS_LINEBREAK(ch) (ch == '\n')
#define IS_BLANK(ch) (ch == ' ' || ch == '\t')
#define IS_COMMENT(ch) (ch == '#')
#define IS_EOF(ch) (ch == EOF)
#define IS_INTERROGATION(ch) (ch == '?')

int currLine = 1;

t_token parserNextToken(FILE * in, char ** secToken) {

	char ch;
	int state;
	static char usefullString[1024];
	int usefullLength;

	usefullLength = 0;
	usefullString[0] = 0;
	* secToken = usefullString;
	state = 0;
	while(1) {

		ch = fgetc(in);

		if (IS_LINEBREAK(ch)) currLine++;

		switch(state) {

			case 0:

				if (IS_DIGIT(ch) || IS_DOT(ch)) {

					state = 1;
					usefullString[0] = ch;
					usefullString[1] = 0;
					usefullLength = 1;
					break ;
				}

				if (IS_LETTER(ch)) {

					state = 2;
					usefullString[0] = ch;
					usefullString[1] = 0;
					usefullLength = 1;
					break ;
				}

				if (IS_COMMENT(ch)) {

					state = 3;
					break ;
				}

				if (IS_BLANK(ch) || IS_LINEBREAK(ch)) {

					break ;
				}

				if (IS_EOF(ch)) {

					return(TOKEN_EOF);
				}

				if (IS_INTERROGATION(ch)) {

					return(TOKEN_END_PATH);
				}

				return(TOKEN_ERROR);
			
			case 1:
	
				if (IS_DIGIT(ch)) {

					usefullString[usefullLength] = ch;
					usefullLength++;
					usefullString[usefullLength] = 0;
					break;
				}

				if (IS_DOT(ch)) {

					usefullString[usefullLength] = ch;
					usefullLength++;
					usefullString[usefullLength] = 0;
					state = 4;
					break;
				}

				if (IS_LETTER(ch) || IS_COMMENT(ch) || IS_BLANK(ch) || IS_LINEBREAK(ch) || IS_EOF(ch)) {

					ungetc(ch, in);
					if (IS_LINEBREAK(ch)) currLine--;
					return(TOKEN_INTEGER);
				}

				return(TOKEN_ERROR);

			case 2:

				if (IS_LETTER(ch)) {

					usefullString[usefullLength] = ch;
					usefullLength++;
					usefullString[usefullLength] = 0;
					break ;
				}

				if (IS_DIGIT(ch) || IS_COMMENT(ch) || IS_BLANK(ch) || IS_LINEBREAK(ch) || IS_EOF(ch)) {

					ungetc(ch, in);
					if (IS_LINEBREAK(ch)) currLine--;
					if (!strcmp(usefullString, "Links")) return(TOKEN_LINKS_KEYWORD);
					if (!strcmp(usefullString, "Paths")) return(TOKEN_PATHS_KEYWORD);

					return(TOKEN_ERROR);
				}

			case 3:

				if (IS_LINEBREAK(ch) || IS_EOF(ch)) {

					state = 0;
					break ;
				}

				break ;

			case 4:
			
				if (IS_DIGIT(ch)) {

					usefullString[usefullLength] = ch;
					usefullLength++;
					usefullString[usefullLength] = 0;
					break;
				}

				if (IS_DOT(ch) || IS_LETTER(ch) || IS_COMMENT(ch) || IS_BLANK(ch) || IS_LINEBREAK(ch) || IS_EOF(ch)) {

					ungetc(ch, in);
					if (IS_LINEBREAK(ch)) currLine--;
					return(TOKEN_REAL);
				}

				return(TOKEN_ERROR);
		}
	}
}

t_graph * parserParse(char * filename, t_list ** paths, t_list ** flt, t_list ** ftx) {

	int state;
	char * secToken = NULL;
	t_token t;
	FILE * in;
	int end = 0;
	int n_nodes = 0;
	int * destNode;
	struct {

		int linkHead;
		int linkTail;
		float linkWeight;
	} link, * newLink;
	t_list * links;
	t_graph * graph;
	t_list * newPath;
	int * flowTime;
	int * txTime;

	* paths = listNew();
	* flt = listNew();
	* ftx = listNew();

	if ((in = fopen(filename, "r")) == 0) {
		
		fprintf(stderr, "Failed to open file '%s' for reading.\n", filename);
		exit(-1);
	}

	links = listNew();
	state = 0;

	while(!end) {

		t = parserNextToken(in, & secToken);

		switch(state) {

			case 0:

				if (t == TOKEN_LINKS_KEYWORD) {

					state = 1;
					break ;
				}

				if (t == TOKEN_PATHS_KEYWORD) {

					newPath = listNew();
					listAdd(* paths, newPath);

					state = 2;
					break ;
				}

				if (t == TOKEN_EOF) {

					end = 1;
					break ;
				}

				fclose(in);
				fprintf(stderr, "Error at line %d: expected keyword or EOF.\n", currLine);
				exit(-2);

			case 1:
				
				if (t == TOKEN_INTEGER) {

					link.linkHead = atoi(secToken);

					if (n_nodes <= link.linkHead) n_nodes = link.linkHead + 1;

					state = 4;
					break ;
				}

				if (t == TOKEN_PATHS_KEYWORD) {

					newPath = listNew();
					listAdd(* paths, newPath);

					state = 2;
					break ;

				}

				if (t == TOKEN_EOF) {

					end = 1;
					break ;
				}

				fclose(in);
				fprintf(stderr, "Error at line %d: expected link source, keyword or EOF.\n", currLine);
				exit(-3);

			case 2:

				if (t == TOKEN_INTEGER) {

					MALLOC(destNode, sizeof(int));
					* destNode = atoi(secToken);
					listAdd(newPath, destNode);
				
					if (n_nodes <= * destNode) n_nodes = * destNode + 1;

					state = 2;
					break ;
				}

				if (t == TOKEN_END_PATH) {

					state = 6;
					break ;
				}

				fclose(in);
				fprintf(stderr, "Error at line %d: expected node identifier or '?'.\n", currLine);
				exit(-4);

			case 3:

				if (t == TOKEN_LINKS_KEYWORD) {

					state = 1;
					break ;
				}

				if (t == TOKEN_INTEGER) {

					newPath = listNew();
					listAdd(* paths, newPath);

					MALLOC(destNode, sizeof(int));
					* destNode = atoi(secToken);
					listAdd(newPath, destNode);
				
					if (n_nodes <= * destNode) n_nodes = * destNode + 1;

					state = 2;
					break ;
				}

				if (t == TOKEN_EOF) {

					end = 1;
					break ;
				}

				fclose(in);
				fprintf(stderr, "Error at line %d: expected keyword or EOF.\n", currLine);
				exit(-2);



			case 4:

				if (t == TOKEN_INTEGER) {

					link.linkTail = atoi(secToken);

					if (n_nodes <= link.linkTail) n_nodes = link.linkTail + 1;

					state = 5;
					break ;
				}

				fclose(in);
				fprintf(stderr, "Error at line %d: expected link destination.\n", currLine);
				exit(-6);
			
			case 5:

				if (t == TOKEN_INTEGER) {

					link.linkWeight = atoi(secToken);
					MALLOC(newLink, sizeof(link));
					* newLink = link;
					listAdd(links, newLink);
					state = 1;
					break ;
				}

				if (t == TOKEN_REAL) {

					link.linkWeight = atof(secToken);
					MALLOC(newLink, sizeof(link));
					* newLink = link;
					listAdd(links, newLink);
					state = 1;
					break ;
				}

				fclose(in);
				fprintf(stderr, "Error at line %d: expected link weight.\n", currLine);
				exit(-7);

			case 6: //FlowTimes
				if (t == TOKEN_INTEGER) {

					MALLOC(flowTime, sizeof(int));
					* flowTime = atoi(secToken);
					listAdd(* flt, flowTime);

					state = 7;
					break ;
				}

				fclose(in);
				fprintf(stderr, "Error at line %d: expected flow time.\n", currLine);
				exit(-8);

			case 7: //TxTime
				if (t == TOKEN_INTEGER) {

					MALLOC(txTime, sizeof(int));
					* txTime = (int) atof(secToken);
					listAdd(* ftx, txTime);

					state = 3;
					break ;
				}

				fclose(in);
				fprintf(stderr, "Error at line %d: expected packet TX duration.\n", currLine);
				exit(-9);
		}
	}

	fclose(in);

	if (listLength(* paths) == 0) {

		fprintf(stderr, "No path specified!\n");
		exit(-8);
	}

	graph = graphNew(n_nodes);
	for (newLink = listBegin(links); newLink; newLink = listNext(links)) {

		graphAddLink(graph, newLink->linkHead, newLink->linkTail, newLink->linkWeight);
		free(newLink);
	}

	listFree(links);
	free(links);

	return(graph);
}

