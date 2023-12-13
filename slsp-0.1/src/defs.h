/* Definições de parâmetros importantes
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
 
#ifndef __DEFS_H__
#define __DEFS_H__

#include <stdlib.h>
#include <semaphore.h>
#include <arpa/inet.h>

/*
 * Definicao de tipos.
 */

/**
 * Tipos de variáveis possíveis no arquivo de configuração.
 */
typedef enum {

	TYPE_FLOAT,
	TYPE_STRING,
	TYPE_INTEGER,
	TYPE_IP_LIST
} t_assign_type;

/**
 * Tipo de dados para representar um endereço IP.
 */
typedef in_addr_t t_ip_addr;

/**
 * Tipo de dados para representar um nó de uma lista de IPs.
 * Engloba também endereços de sub-redes (com máscara).
 */
typedef struct {

	/**
	 * Endereço IP.
	 */
	t_ip_addr 	ip;

	/**
	 * Máscara (se aplicável);
	 */
	unsigned char 	mask;

	/**
	 * Número de IPs na lista (o primeiro nó da lista guarda o valor correto).
	 */
	unsigned int 	number_of_entries;

	/**
	 * Ponteiro para o próximo elemento da lista.
	 */
	void * next;

} t_ip_list;


 /****************************************************************
 *                     Variáveis Exportadas                      *
 ****************************************************************/

extern t_ip_addr ifce_ipaddr;

extern char ifce_name[7];

extern int ifce_index;

extern t_ip_addr ifce_bcastaddr;

extern unsigned int hello_life_time;

extern unsigned int topology_life_time;

extern unsigned int port;

extern size_t pkt_max_size;

extern unsigned int hello_intervall;

extern unsigned int topology_intervall;

extern float aging;

extern unsigned char lq_level;

extern char insmod_path[50];

extern char rmmod_path[50];

extern char pprs_path[50];

extern char iptables_path[50];

extern char ip_path[50];

extern char length_mod_path[50];

extern char pprs_classes_file[50];

extern char pprs_rates_file[50];

extern char per_table_path[50];

extern unsigned int daemonize;

extern unsigned int topologyView;

extern t_ip_list * sub_nets;

/* Variáveis de controle. */

extern unsigned char run;

extern unsigned char status;

extern unsigned int seq_number_threshold;

extern sem_t send_sock_mutex;

extern sem_t finalize_mutex;

extern sem_t neigh_mutex;

extern sem_t topo_mutex;

/* Outras variaveis. */

/**
 * Tamanho máximo para a string que guarda a data de início
 * da execução do protocolo.
 */
#define DATE_STRING_LEN         30

extern char start_date[DATE_STRING_LEN];

/*
 * Funcoes de manipulacao destes parametros.
 */

void init_var_pool();

int find_variable(char *, t_assign_type *);

void assign_variable_value(int , void *);

void validate_variables();

/*
 * Liberacao de memoria do modulo.
 */

void free_defs();

#endif

