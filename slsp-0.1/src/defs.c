/**
 * @file
 * @author Diego Passos <dpassos@ic.uff.br>
 * @version 1.0
 *
 * @section DESCRIPTION
 *
 * Módulo que armazena definições globais utilizadas por todo o protocolo. As variáveis 
 * controladas pelo arquivo de configuração também são declaradas e manipuladas aqui.
 */


#include "defs.h"
#include "util.h"
#include "net.h"
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

/*
 * Variaveis globais.
 */

/**
 * Endereço IP da interface na qual o protocolo é executado.
 */
t_ip_addr ifce_ipaddr;

/**
 * Nome da interface na qual o protocolo é executado.
 */
char ifce_name[7] = "eth1";

/**
 * Índice da interface de rede (dentro do kernel).
 */
int ifce_index = -1;

/**
 * Endereço de broadcast da interface na qual o protocolo é executado.
 */
t_ip_addr ifce_bcastaddr;

/**
 * Tempo de vida (validade) de um pacote de hello.
 */
unsigned int hello_life_time = 15;

/**
 * Tempo de vida (validade) de um pacote de topologia.
 */
unsigned int topology_life_time = 10;

/**
 * Porta padrão do protocolo.
 */
unsigned int port = 255;

/**
 * Tamanho máximo de um pacote de controle.
 */
size_t pkt_max_size = 8000;

/**
 * Intervalo entre envios de pacotes de hello.
 */
unsigned int hello_intervall = 5;

/**
 * Fator de envelhecimento da métrica (recíproco do tamanho da janela).
 */
float aging = 0.1;

/**
 * Métrica escolhida:
 *  - 0: Hop Count.
 *  - 1: ETX.
 *  - 2: ML.
 *  - 3: MARA
 *  - 4: MARA-P.
 *  - 5: MARA-RP
 */
unsigned char lq_level = 1;

/**
 * Intervalo entre envios de pacotes de topologia.
 */
unsigned int topology_intervall = 10;

/**
 * Caminho para o programa insmod.
 */
char insmod_path[50] = "/sbin/insmod";

/**
 * Caminho para o programa rmmod.
 */
char rmmod_path[50] = "/sbin/rmmod";

/**
 * Caminho para o módulo pprs.
 */
char pprs_path[50] = "/lib/modules/2.4.30/pprs.o";

/**
 * Caminho para o programa iptables.
 */
char iptables_path[50] = "/usr/sbin/iptables";

/**
 * Caminho para o programa ip.
 */
char ip_path[50] = "/usr/sbin/ip";

/**
 * Caminho para o módulo ipt_length.
 */
char length_mod_path[50] = "/lib/modules/2.4.30/ipt_length.o";

/**
 * Caminho para o arquivo pprs_classes.
 */
char pprs_classes_file[50] = "/proc/pprs_classes";

/**
 * Caminho para o arquivo pprs_rates.
 */
char pprs_rates_file[50] = "/proc/pprs";

/**
 * Caminho para o arquivo com a tabela de conversão de PER.
 */
char per_table_path[50] = "/etc/perTable.csv";

/**
 * Opção booleana para determinar se o protocolo deve ou não
 * executar em background.
 */
unsigned int daemonize = 1;

/**
 * Opção booleana para determinar se o protocolo deve ou não
 * disponibilizar o serviço de envio de topologia.
 */
unsigned int topologyView = 1;

/**
 * Variável de controle para as threads. Quando zero, todas as threads
 * encerram seus loops.
 */
unsigned char run = 1;

/**
 * Variável global para tratamento de códigos de erros.
 */
unsigned char status = 0;

/**
 * Limiar para determinar se um pacote com número de sequencia menor que o último é novo
 * (por causa de overflow) ou antigo (pacote chegou atrasado).
 */
unsigned int seq_number_threshold = 3;

/**
 * Lista de sub-redes anunciadas por este nó.
 */
t_ip_list * sub_nets = NULL;

/**
 * Mutex para controlar o acesso ao socket de envio de pacotes de controle.
 */
sem_t send_sock_mutex;

/**
 * Mutex que bloqueia a thread principal. Esta thread espera por um desbloqueio para
 * encerrar a execução do protocolo.
 */
sem_t finalize_mutex;

/**
 * Mutex para controlar o acesso à estrutura de dados da lista de vizinhos.
 */
sem_t neigh_mutex;

/**
 * Mutex para controlar o acesso à estrutura de dados da topologia.
 */
sem_t topo_mutex;

/**
 * String que guarda a data de início da execução do protocolo (para fins de debug).
 */
char start_date[DATE_STRING_LEN];

/**
 * Tamanho máximo do nome de uma variável no arquivo de configuração.
 */
#define MAX_VAR_NAME		40

/**
 * Tamanho máximo do pool de variáveis do arquivo de configuração.
 */
#define MAX_VAR_POOL_SIZE	40

/**
 * Tipo de dados que representa uma variável lida do arquivo de configuração.
 */
typedef struct {

	/**
	 * Nome da variável.
	 */
	char 			name[MAX_VAR_NAME];

	/**
	 * Tipo da variável.
	 */
	t_assign_type		type;

	/**
	 * Ponteiro para a posição de memória que guarda o valor
	 * da variável.
	 */
	void *			location;

} t_var_entry;

/**
 * Pool de variáveis.
 */
t_var_entry var_pool[MAX_VAR_POOL_SIZE];

/**
 * Número de variáveis no pool.
 */
int var_pool_entries = 0;

/**
 * Insere uma nova variável no pool.
 * @param var Variável.
 * @param vartype Tipo de variável.
 * @return Nada.
 */
#define PUTVAR(var, vartype)	if (var_pool_entries == MAX_VAR_POOL_SIZE) {\
					fprintf(stderr, "Too many variables in the pool.\n");\
					exit(1);\
				}\
				strcpy(var_pool[var_pool_entries].name, #var);\
				var_pool[var_pool_entries].type = vartype;\
				var_pool[var_pool_entries++].location = (void *) & var

/*
 * Implementacao das funcoes.
 */

/**
 * Inicializa o pool de variáveis.
 * @return Nada.
 */
void init_var_pool() {

	PUTVAR(ifce_name, TYPE_STRING);
	PUTVAR(hello_life_time, TYPE_INTEGER);
	PUTVAR(topology_life_time, TYPE_INTEGER);
	PUTVAR(port, TYPE_INTEGER);
	PUTVAR(hello_intervall, TYPE_INTEGER);
	PUTVAR(topology_intervall, TYPE_INTEGER);
	PUTVAR(aging, TYPE_FLOAT);
	PUTVAR(lq_level, TYPE_INTEGER);
	PUTVAR(insmod_path, TYPE_STRING);
	PUTVAR(rmmod_path, TYPE_STRING);
	PUTVAR(pprs_path, TYPE_STRING);
	PUTVAR(iptables_path, TYPE_STRING);
	PUTVAR(ip_path, TYPE_STRING);
	PUTVAR(length_mod_path, TYPE_STRING);
	PUTVAR(pprs_classes_file, TYPE_STRING);
	PUTVAR(pprs_rates_file, TYPE_STRING);
	PUTVAR(per_table_path, TYPE_STRING);
	PUTVAR(daemonize, TYPE_INTEGER);
	PUTVAR(topologyView, TYPE_INTEGER);
	PUTVAR(sub_nets, TYPE_IP_LIST);
}

/**
 * Busca uma variável por nome.
 * @param varName Nome da variável buscada.
 * @param type Ponteiro que armazena o tipo da variável, caso a mesma seja encontrada. Valor
 * é indefinido caso contrário.
 * @return Retorna o índice da variável no pool, caso ela exista. Retorna -1, caso contrário.
 */
int find_variable(char * varName, t_assign_type * type) {

	int i;

	for (i = 0; i < var_pool_entries; i++) {

		if (!strcmp(var_pool[i].name, varName)) {

			* type = var_pool[i].type;
			return(i);
		}
	}

	return(-1);
}

/**
 * Atribui um valor a uma variável.
 * @param poolIndex Índice da variável no pool.
 * @param value Ponteiro para área de memória contendo o valor a ser atribuído.
 * @return Nada.
 * @note A função automaticamente determina o tamanho do dado a ser atribuído.
 */
void assign_variable_value(int poolIndex, void * value) {

	unsigned int oc[4];
	unsigned int mask, complMask;
	unsigned char ocByte[4];
	t_ip_addr * ip;

	switch(var_pool[poolIndex].type) {

		case TYPE_STRING:

			strcpy((char *) var_pool[poolIndex].location, (char *) value);
			break ;

		case TYPE_FLOAT:

			* ((float *) var_pool[poolIndex].location) = * ((float *) value);
			break ;

		case TYPE_INTEGER:

			* ((int *) var_pool[poolIndex].location) = * ((int *) value);
			break ;

		case TYPE_IP_LIST:

			/*
			 * Fazer parse da string. Vamos guardar todos os enderecos em NBO (Network Byte Order) para
			 * simplificar o processamento no restante do protocolo.
			 */

			sscanf((char *) value, "%u.%u.%u.%u/%u", & (oc[0]), & (oc[1]), & (oc[2]), & (oc[3]), & mask);

			/*
			 * Testar a validade dos octetos e da mascara.
			 */

			if (oc[0] > 255 || oc[1] > 255 || oc[2] > 255 || oc[3] > 255 || mask >= 32) {

				fprintf(stderr, "Sub-rede invalida: %s.\n", (char *) value);
				exit(1);
			}

			ocByte[0] = oc[0];
			ocByte[1] = oc[1];
			ocByte[2] = oc[2];
			ocByte[3] = oc[3];

			/*
			 * Verificar se a mascara faz sentido para esta sub-rede. Atencao para as conversoes de NBO
			 * para HBO (Host Byte Order). Elas simplificam as manipulacoes realizadas aqui.
			 */

			ip = (t_ip_addr *) ocByte;
			if (htonl((* ip)) << mask) {

				fprintf(stderr, "Mascara %hhu nao faz sentido para a sub-rede %hhu.%hhu.%hhu.%hhu.\n", 
						mask, ocByte[0], ocByte[1], ocByte[2], ocByte[3]);
				complMask = 32 - mask;
				(* ip) = ntohl((htonl((* ip)) >> complMask) << complMask);
				fprintf(stderr, "Voce quis dizer %hhu.%hhu.%hhu.%hhu/%hhu?\n", ocByte[0], ocByte[1], ocByte[2], ocByte[3], mask);
				exit(1);
			}

			/*
			 * Criar o novo no da lista.
			 */

			t_ip_list * new_ip;

			new_ip = (t_ip_list *) p_alloc(sizeof(t_ip_list));
			new_ip->ip = (* ip);
			new_ip->mask = mask;

			/*
			 * Adicionar a lista.
			 */

			new_ip->next = * ((t_ip_list **) var_pool[poolIndex].location);
			if (new_ip->next)
				new_ip->number_of_entries = (* ((t_ip_list **) var_pool[poolIndex].location))->number_of_entries + 1;
			else
				new_ip->number_of_entries = 1;
			* ((t_ip_list **) var_pool[poolIndex].location) = new_ip;

			break ;
	}

}

/**
 * Verifica validade (consistência) dos valores das variáveis do pool. 
 * @return Nada.
 * @note Esta função é chamada imediatamente após a análise sintática do arquivo de configuração.
 * Caso algum valor inválido seja encontrado, uma mensagem de erro é impressa e o programa é 
 * abortado.
 */
void validate_variables() {

	/*
	 * Testes de valores validos para cada variavel devem ser colocados nesta funcao.
	 */

	if (ifce_name[0] == 0 || (ifce_index = get_ifce_index(ifce_name)) < 0) {

		fprintf(stderr, "Nome da interface eh invalido.\n");
		exit(1);
	}

	if (port > 65535) {

		fprintf(stderr, "Porta especificada eh invalida.\n");
		exit(1);
	}

	if (lq_level > 5) {
		
		fprintf(stderr, "Nivel de qualidade de enlaces invalido. Valores validos: de 0 a 5.\n");
		exit(1);
	}
	
	if (hello_intervall == 0 || hello_intervall > 60) {
		
		fprintf(stderr, "Intervalo de envio de hello invalido. Valores validos: de 1 a 60.\n");
		exit(1);
	}

	if (topology_intervall == 0 || topology_intervall > 120) {
		
		fprintf(stderr, "Intervalo de envio de hello invalido. Valores validos: de 1 a 120.\n");
		exit(1);
	}
	
	if (hello_life_time < hello_intervall) {

		fprintf(stderr, "Tempo de vida do pacote de hello nao pode ser menor que o intervalo de envio.\n");
		exit(1);
	}

	if (topology_life_time < topology_intervall) {

		fprintf(stderr, "Tempo de vida do pacote de topologia nao pode ser menor que o intervalo de envio.\n");
		exit(1);
	}

	if (aging <= 0.0 || aging >= 1.0) {

		fprintf(stderr, "Valor invalido para a variavel aging. Valores validos: no intervalo (0.0; 1.0).\n");
		exit(1);
	}

	if ((1/aging) > hello_life_time) {

		fprintf(stderr, "Valor invalido para a variavel aging. O inverso do aging deve ser menor ou igual ao hello_life_time.\n");
		exit(1);
	}
}

/**
 * Libera a memória alocada dinamicamente por este módulo.
 * @return Nada.
 */
void free_defs() {

	t_ip_list * p;

	while(sub_nets) {

		p = sub_nets->next;
		p_free(sub_nets);
		sub_nets = p;
	}
}

