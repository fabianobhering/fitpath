/*   protocolo - Protocolo de roteamento pró-ativo para 
 * redes sem fio de múltiplos saltos, baseado em estado 
 * de enlaces.
 *
 *   util.h - Cabeçalho do módulo de utilidades. Funções
 * de manipulação de memória e tratamento de erros.
 *
 *   Por Diego Passos, 2007.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef _UTIL_H
#define _UTIL_H 1

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

#include "defs.h"

/****************************************************************
 *                           Constantes                         *
 ****************************************************************/


/* Macros para evitar problemas de diferencas de precisao em ponto
 * flutuante em arquiteturas diferentes.
 */

/**
 * Máxima precisão considerada entre arquiteturas.
 */
#define MAX_PRECISION		0.001

/**
 * Verifica se dois valores reais são iguais (dentro da precisão especificada).
 * @param x Primeiro valor.
 * @param y Segundo valor.
 * @return 1, se valores são iguais. 0, caso contrário.
 */
#define FLOAT_IS_EQUAL(x,y)	((x < y + MAX_PRECISION) && (x > y - MAX_PRECISION))

#ifdef	DEBUG

#define MAX_DEBUG_MSG_SIZE	100	// Tamanho máximo das strings de debug.

#endif

typedef enum {sysctl_error, setsockopt_error, out_of_memory, double_free, 
				socket_error, bind_error, info_mismatch, file_open} t_error_type;

typedef enum {alloc_not_empty, unknown_pkt, checksum_fail, no_graph_node,
				bipart_graph, bad_route} t_warning_type;


#if 0
/****************************************************************
 *                        Tipos de Dados                        *
 ****************************************************************/

/* Definição de um nó de uma tabela de alocação. */

typedef struct {
	void * 			addr;
	unsigned int		size;
	char			func[50];
	char			file[50];
	unsigned short		line;
	struct t_alloc_node *	next;
	} t_alloc_node;

/****************************************************************
 *                            Funções                           *
 ****************************************************************/
#endif

void p_error(t_error_type, const char *);

void p_warning(t_warning_type, const char *);

#ifdef	DEBUG
	
void d_printf(char *, const char *);

#endif

#if 0
#define p_alloc(x)	__p_alloc__(x, __func__, __FILE__, __LINE__)
void *__p_alloc__(unsigned int, const char *, const char *, unsigned short);

#define p_free(addr) __p_free(addr, __FILE__, __LINE__)
void __p_free(void *, char *, int);
#endif

void * p_alloc(unsigned int);

/**
 * Fora do modo de debug, podemos usar a função free normal.
 */
#define p_free(addr)	free(addr)

#if 0
void free_alloc_table(void);
#endif

char * loseExt(char *);
char * steam(char *);

char * get_current_time(char * );

#endif

