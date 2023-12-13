/* Módulo responsável pela manipulação de pacotes
 * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "topology.h"

#include <string.h>

/****************************************************************
 *                           Constantes                         *
 ****************************************************************/

/*
 * Tamanho da parte de dados do pacote de hello. Queremos pacotes
 * de 1500 bytes. Logo, devemos ter 1500 - protocolo_hdr - udp_hdr.
 */

/**
 * Tamanho de um pacote de hello (em bytes).
 */
#define HELLO_PKT_SIZE	(1492 - sizeof(t_packet))

/**
 * Tipos de pacote de controle usados pelo protocolo.
 */
typedef enum {hello_type, topology_type} t_pkt_type; 


/****************************************************************
 *                              Macros                          *
 ****************************************************************/

/**
 * Calcula o próximo número de sequência.
 * @param n Número de sequência atual.
 * @return Próximo número de sequencia.
 */
#define next_seq(n) (n + 1)

/**
 * Valor máximo do um número de sequência.
 */
#define MAX_SEQ_NUMBER	((t_seq) -1)

/****************************************************************
 *                        Tipos de Dados                        *
 ****************************************************************/

/**
 * Tipo de dados usado para representar números de sequência.
 */
typedef unsigned int t_seq;	// Tipo de número de seqüência

/*
 * Cabecalho dos pacotes de controle. LEMBRAR: qualquer alteracao 
 * feita nesta estrutura deve ser coerente com a constante 
 * HELLO_PKT_SIZE. Ou seja, o tamanho do cabecalho deve ser atualizado
 * dentre os bytes retirados do tamanho do pacote de hello.
 */

/**
 * Cabeçalho de um pacote de controle.
 */
typedef struct __attribute__ ((__packed__)) {

	/**
	 * Tipo do pacote.
	 */
	t_pkt_type		type;

	/**
	 * Número de sequencia do pacote.
	 */
	t_seq			seq;

	/**
	 * Taxa de transmissão utilizada para enviar este pacote.
	 */
	unsigned char		rate;

	/**
	 * Tamanho da seção de dados do pacote.
	 */
	unsigned long		data_size;

	/**
	 * Checksum do pacote.
	 */
	unsigned int		checksum;

	/**
	 * Ponteiro para a seção de dados.
	 */
	void *			data;
} t_packet;	
	
/**
 * Estrutura de um nó na lista de nós conhecidos na topologia da rede.
 */
typedef struct {

	/**
	 * Endereço IP do nó.
	 */
	t_ip_addr		  ip;

	/**
	 * Número de sequência do último pacote de tc recebido deste nó.
	 */
	unsigned int		  last_seq;

	/**
	 * Contador de tempo de validade das informações do nó.
	 */
	unsigned int		  count;

	/**
	 * Ponteiro para o próximo da lista.
	 */
	struct t_tc_node	* next;

} t_tc_node;

/****************************************************************
 *                            Funções                           *
 ****************************************************************/

/* Funções de geração de pacotes. */
	
t_packet *gen_hello_pkt(unsigned char rate);

t_packet *gen_topology_pkt(void);
	
/* Funções de parsing de pacotes. */
	
int parse_pkt(t_packet *, t_ip_addr);
	
void parse_hello_pkt(t_packet *, t_ip_addr);
	
void parse_topology_pkt(t_packet *, t_ip_addr);
	
/* Funções de transformações de pacotes. */
	
unsigned char * serialize_pkt(t_packet *, size_t *);

t_packet * deserialize_pkt(unsigned char *, size_t);

/* Funções auxiliares. */
	
unsigned int checksum(t_packet *);

void free_pkt(t_packet *);

t_tc_node * find_tc_node(t_ip_addr);

void free_tc_list(void);

void update_tc_list(void);
