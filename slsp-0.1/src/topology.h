/* Módulo responsável pela manipulação da topologia
 * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef __TOPOLOGY_H__
#define __TOPOLOGY_H__

#include "net.h"
#include "mara.h"

#include <string.h>


/****************************************************************
 *                        Tipos de Dados                        *
 ****************************************************************/

/* Data de expiracao de um pacote de hello. */

/**
 * Tipo que representa uma lista de ttls.
 */
typedef struct tmp_t_ttl {

	/**
	 * ttls (em unidades de tempo do protocolo).
	 */
	unsigned short			ttl;

	/**
	 * Próximo na lista.
	 */
	struct tmp_t_ttl *		next;
} t_ttl;

/* Janela de qualidade do enlace. */

/**
 * Estrutura de dados que guarda informações sobre a qualidade dos enlaces.
 */
typedef struct {

	/**
	 * Ponteiro para o início da lista de ttls.
	 */
	t_ttl *				next; /* Ponteiro para o comeco da lista (pacote mais antigo). */

	/**
	 * Poteiro para o fim da lista de ttls.
	 */
	t_ttl * 			last; /* Ponteiro para o ultimo (apenas para a insercao). */

	/**
	 * Tempo desde o último pacote de hello inserido na janela.
	 */
	unsigned short			since_last; /* Tempo desde o ultimo pacote inserido. */

	/**
	 * Número de pacotes na janela.
	 */
	unsigned short			count; /* Numero de pacotes na janela. */

	/**
	 * Probabilidade calculada pelo vizinho.
	 */
	float				nlq_prob;

	/**
	 * Probabilidade calculada pelo nó local.
	 */
	float				lq_prob;
} t_window;

/* Nó da lista de vizinhos. */

/**
 * Estrutura que representa um nó vizinho.
 */
typedef struct {

	/**
	 * Endereço IP do nó.
	 */
	t_ip_addr			ip;

	/**
	 * Contador de tempo de validade da informações.
	 */
	unsigned short			count;

	/**
	 * Vetor de custos (um para cada taxa de transmissão).
	 */
	float				cost[4];

	/**
	 * Status do nó, simétrico ou assimétrico.
	 */
	unsigned char			simmetric;

	/**
	 * Janelas de pacotes (uma para cada taxa).
	 */
	t_window			lq_window[4]; /* Janelas de pacotes de hello. */

	/**
	 * Ponteiro para o próximo.
	 */
	struct t_neigh_node		*next;
} t_neigh_node;
	
/* Formato dos campos (cada vizinho) no pacote de hello. */

/**
 * Formato de uma entrada para enlace no pacote de hello.
 */
typedef struct {

	/**
	 * Endereço IP do vizinho.
	 */
	t_ip_addr			ip;

	/**
	 * Valores das probabilidades computadas (uma para cada taxa).
	 */
	float				lq[4]; /* 4 por causa do MARA. */
} t_hello_entry;

/* Informações do enlace: nó de destino, métrica, etc... */

/**
 * Informação de enlace para envio no pacote de topologia.
 */
typedef struct {

	/**
	 * Endereço IP do vizinho.
	 */
	t_ip_addr			ip;

	/**
	 * Vetor de custos (um para cada taxa).
	 */
	float				cost[4];
	// TODO: adicionar outros campos.
} t_link_info;

/* Informacoes sobre HNA no pacote de topologia. */

/**
 * Informações de sub-redes exportadas.
 */
typedef struct {

	/**
	 * Endereço IP da sub-rede.
	 */
	t_ip_addr			ip;

	/**
	 * Máscara de sub-rede.
	 */
	unsigned char			mask;
} t_HNA_info;

/* Grafo da topologia é uma lista encadeada desta estrutura. */

/**
 * Nó de uma lista temporária de nós da rede.
 */
typedef struct tmp_t_graph_node {

	/**
	 * Endereço IP do nó.
	 */
	t_ip_addr			ip;

	/**
	 * Máscara de sub-rede.
	 */
	unsigned char			mask;

	/**
	 * Próximo salto até o nó.
	 */
	t_ip_addr			next_hop;

	/**
	 * Número de enlaces no caminho até o nó.
	 */
	unsigned int		 	n_links;

	/**
	 * Número de HNAs providos por este nó.
	 */
	unsigned int		 	n_HNAs;

	/**
	 * Custo do caminho até o nó.
	 */
	float				path_cost;

	/**
	 * Número de saltos até o nó.
	 */
	unsigned int 			path_hops;

	/**
	 * Próximo na lista.
	 */
	struct tmp_t_graph_node		* next;

	/**
	 * Ponteiro para a lista de enlaces do nó.
	 */
	t_link_info			* links;

	/**
	 * Ponteiro para a lista de HNAs do nó.
	 */
	t_HNA_info			* HNA;
} t_graph_node;
	
/*
 * Variaveis exportadas.
 */

extern t_graph_node * graph;

/****************************************************************
 *                            Funções                           *
 ****************************************************************/

/* Manipulação da lista de vizinhos. */
	
unsigned char add_neigh(t_ip_addr, unsigned char);

unsigned char update_neigh(void);
	
unsigned char check_simmetry_and_read_lq(t_ip_addr, void *);

void free_neigh(void);
	
t_neigh_node * find_neigh(t_ip_addr);

void compute_metric_for_neighs();

void * gen_neigh_data(unsigned long *);

void gen_hello_data(void *);

void add_to_graph(t_link_info *, size_t, unsigned char);
	
void rm_from_graph(t_ip_addr);

unsigned int find_graph_node_pos(t_ip_addr);

void free_graph(void);

void dijkstra(void);

void append_tmp_graph_node(t_graph_node *, t_graph_node **);

void free_tmp_graph(t_graph_node *);

t_graph_node * find_tmp_graph_node(t_ip_addr, t_graph_node *);

t_graph_node * remove_tmp_graph_node(t_ip_addr, t_graph_node **);

#endif

