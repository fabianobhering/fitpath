/**
 * @file
 * @author Diego Passos <dpassos@ic.uff.br>
 * @version 1.0
 *
 * @section DESCRIPTION
 *
 * Módulo que implementa rotinas de manipulação de pacotes de controle
 * do protocolo.
 */


#include "packets.h"

/****************************************************************
 *                      Variáveis Globais                       *
 ****************************************************************/

/**
 * Próximo número de sequência para pacotes de hello.
 */
static t_seq hello_seq = 0;

/**
 * Próximo número de sequência para pacotes de topologia.
 */
static t_seq topology_seq = 0;

/**
 * Lista de nós da topologia (dos quais recebemos mensagens de TC).
 */
static t_tc_node * tc_list = NULL;


/****************************************************************
 *                        Implementações                        *
 ****************************************************************/

/**
 * Gera um novo pacote de hello para ser transmitido em uma dada taxa.
 * @param rate Índice da taxa de transmissão a ser usada.
 * @return Ponteiro para uma estrutura do pacote criado.
 */
t_packet *gen_hello_pkt(unsigned char rate) {

	t_packet *hello_pkt;
	
	/* Alocar memória para o pacote. */

	hello_pkt = p_alloc(sizeof(t_packet));
	
	/* Preencher os campos. */

	hello_pkt->type = hello_type;
	hello_pkt->seq = hello_seq = next_seq(hello_seq);
	hello_pkt->checksum = 0;	// O checksum real será calculado mais tarde.
	hello_pkt->rate = rate;
	
	/* Os pacotes de hello tem um tamanho fixo, 
	 * determinado pela constante HELLO_PKT_SIZE. */

	hello_pkt->data_size = HELLO_PKT_SIZE;
	
	/* Alocar memória para os dados do pacote. */
	
	hello_pkt->data = p_alloc(hello_pkt->data_size);

	/* Preencher o conteudo do pacote. */

	gen_hello_data(hello_pkt->data);

	/* Calcular o checksum. */
	
	hello_pkt->checksum = checksum(hello_pkt);
	
	/* Pacote criado com sucesso. */
	
	return(hello_pkt);
}

/**
 * Gera um pacote de topologia para ser transmitido.
 * @return Ponteiro para o pacote recém-criado.
 */
t_packet *gen_topology_pkt(void) {
	
	t_packet 	* topology_pkt;
	
	/* Alocar memória para o pacote. */

	topology_pkt = p_alloc(sizeof(t_packet));
	
	/* Preencher os campos. */

	topology_pkt->type = topology_type;
	topology_pkt->seq = topology_seq = next_seq(topology_seq);
	topology_pkt->checksum = 0;	// O checksum real será calculado mais tarde.
	topology_pkt->rate = 0;
	
	/* Gerar os dados do pacote. */
	
	topology_pkt->data = gen_neigh_data(&(topology_pkt->data_size));
	
	/* Calcular o checksum. */
	
	topology_pkt->checksum = checksum(topology_pkt);
	
	/* Pacote criado com sucesso. */

	return(topology_pkt);
}

/**
 * Realiza o parse inicial de um pacote de controle recebido.
 * @param pkt Ponteiro para o pacote recebido.
 * @param from Endereço da origem do pacote.
 * @return i
 *  - 0, caso o pacote seja válido. 
 *  - -1, caso o pacote tenha checksum inválido.
 *  - -2, caso o tipo de pacote seja inválido.
 */
int parse_pkt(t_packet * pkt, t_ip_addr from){

	/* Testar a integridade do pacote. */
	
	if (checksum(pkt) != 0) {
		p_warning(checksum_fail, "parse_pkt");
		return(-1);
	}		
	
	/* Decidir o tipo de pacote e chamar o parser correspondente. */
	
	switch(pkt->type) {
		case hello_type:
			parse_hello_pkt(pkt, from);
			break;
		case topology_type:
			parse_topology_pkt(pkt, from);
			break;
		default:
			p_warning(unknown_pkt, "parse_pkt");
			return(-2);
		}
	
	/* Parsing sem erros. */
		
	return(0);
}

/**
 * Realiza o parse de um pacote de hello recém-recebido.
 * @param hello_pkt Ponteiro para a estrutura na qual o pacote foi recebido.
 * @param from Endereço do nó de origem do pacote.
 * @return Nada.
 */
void parse_hello_pkt(t_packet * hello_pkt, t_ip_addr from) {
	
	unsigned char change;
	t_packet * topology_pkt;
	unsigned char * serial_topology_pkt;
	size_t topology_pkt_size;
	
	/* Verificar se o pacote foi mandado por outro nó. */
	
	if (from != ifce_ipaddr) {
		
		if (hello_pkt->rate < 4) {

			if (!daemonize) printf("HELLO received from %s with rate %d.\n", inetaddr(from), hello_pkt->rate);

			/* Vamos adicionar o vizinho à lista. */

			change = add_neigh(from, hello_pkt->rate);

			/* E verificar se estamos na lista de vizinhos dele. */

			change = check_simmetry_and_read_lq(from, hello_pkt->data) || change;
		}
		else {

			change  = 0;
		}
	}
	else {
		
		/* O pacote é do nosso próprio nó. Isso marca uma
		 * unidade de tempo. Vamos atualizar a lista de vizinhos. */
		
		change = update_neigh();
	}
	
	if (change) {
		
		/* Houve mudança na topologia. É necessário "avisar" os demais nós. */
		
		topology_pkt = gen_topology_pkt();
		
		serial_topology_pkt = serialize_pkt(topology_pkt, &topology_pkt_size);
		
		p_send(serial_topology_pkt, topology_pkt_size, ifce_bcastaddr);
		
		free_pkt(topology_pkt);
		
		p_free(serial_topology_pkt);
	}
}

/**
 * Realiza o parse de um pacote de topologia recém-recebido.
 * @param topology_pkt Ponteiro para a estrura representando o pacote.
 * @param from Endereço do nó de origem do pacote.
 * @return Nada.
 */
void parse_topology_pkt(t_packet * topology_pkt, t_ip_addr from) {

	t_tc_node 		* source, *p;
	unsigned char	* fwd_pkt;
	size_t			  fwd_pkt_size;
	
	/* Verificar o último pacote recebido deste nó. */
	
	source = find_tc_node(((t_link_info *) topology_pkt->data)[0].ip);
	
	if (source == NULL) {
		
		/* É o primeiro pacote de topologia recebido deste nó. */
		
		source = p_alloc(sizeof(t_tc_node));
		
		source->ip = ((t_link_info *) topology_pkt->data)[0].ip;
		
		source->last_seq = topology_pkt->seq;
		
		source->count = topology_life_time;
		
		/* Vamos inseri-lo na lista. */
		
		if (tc_list == NULL) source->next = NULL;
		else source->next = (struct t_tc_node *) tc_list;

		tc_list = source;

		p = tc_list;

		/* Adicionar as novas informações ao grafo. */
		
		add_to_graph(topology_pkt->data, topology_pkt->data_size, 1);

	}
	else {
	
		/* Verificar se já recebemos este pacote.
		 * Utilizamos um threshold para guarantir que não vamos 
		 * considerar pacotes com número de seqüência menor que
		 * chegam atrasados, como novas informações, quando na 
		 * verdade são antigas. */
		
		if (source->last_seq < seq_number_threshold) {
			if ((topology_pkt->seq <= source->last_seq) ||
				(topology_pkt->seq > MAX_SEQ_NUMBER + source->last_seq - seq_number_threshold)) {
				return ;
			}
		} 
		else {
			if (topology_pkt->seq <= source->last_seq) {
				return ;
			}
		
		}
		
		/* É um novo pacote. */
			
		source->last_seq = topology_pkt->seq;
		
		/* Renovar o tempo de vida. */
		
		source->count = topology_life_time;
		
		/* Adicionar as novas informações ao grafo. */
		
		add_to_graph(topology_pkt->data, topology_pkt->data_size, 0);

	}
	
	if (from != ifce_ipaddr) {
		
		/* Se não for um pacote enviado pelo próprio nó, 
		 * vamos encaminhá-lo aos demais vizinhos. */
		
		fwd_pkt = serialize_pkt(topology_pkt, &fwd_pkt_size);
		
		p_send(fwd_pkt, fwd_pkt_size, ifce_bcastaddr);
		
		/* Liberar memória do pacote criado. */
		
		p_free(fwd_pkt);
		
	}
	else {
		
		/* Caso contrário, vamos atualizar o tempo de vida dos nós. */
		
		update_tc_list();
	}
}
	
/* Função de checksum. Pressupõe que o campo 'checksum' do pacote esteja zerado,
 * caso o pacote esteja sendo criado. */

/**
 * Calcula o checksum de um pacote.
 * @param pkt Pacote para o qual deve-se calcular o checksum.
 * @return Diferença entre a soma calculada agora e o valor do campo checksum.
 * @note Quando utilizada para calcular inicialmente o checksum, esta função supõe 
 * que o campo checksum no pacote esteja zerado.
 */
unsigned int checksum(t_packet * pkt) {
	unsigned char *p;	// Ponteiro para varrer os bytes do pacote.
	unsigned int sum;	// Soma
	size_t pkt_size;	// Tamanho do pacote

	sum = 0;
	
	/* Calcular o tamanho do pacote. */
	
	pkt_size = sizeof(t_packet) - sizeof(void *) - sizeof(unsigned int);
	
	/* Contabilizar os bytes do cabeçalho, exceto o próprio checksum. */
	
	for (p = (unsigned char *) pkt; p < (unsigned char *) pkt + pkt_size; p++)
		sum += (unsigned int) *p;
	
	/* Contabilizar os bytes de dados. */
	
	for (p = pkt->data; p < (unsigned char *) pkt->data + pkt->data_size; p++)
		sum += (unsigned int) *p;
	
	/* O retorno é a soma subtraindo-se o campo 'checksum'. Com isso, na
	 * criação de um novo pacote, o valor é o próprio checksum, enquanto name
	 * verificação é zero, caso esteja correto. */
	
	return(sum - pkt->checksum);	
}

/**
 * Libera a memória alocada de um pacote.
 * @param pkt Ponteiro para o pacote a ser desalocado.
 * @return Nada.
 */
void free_pkt(t_packet * pkt) {
	
	/* Primeiro, desalocamos a memória da parte de dados. */
	
	p_free(pkt->data);
	
	/* Depois, podemos desalocar o pacote em si. */
	
	p_free(pkt);

}

/**
 * Serializa uma estrutura do tipo t_packet em uma sequência de 
 * bytes.
 * @param pkt Ponteiro para a estrutura t_packet do pacote.
 * @param size Tamanho total do pacote.
 * @return Ponteiro para o buffer resultante da serialização.
 * @note A área de memória do pacote serializado é alocada dentro da 
 * própria função e deve ser liberada pelo usuário.
 */
unsigned char * serialize_pkt(t_packet * pkt, size_t * size) {
	unsigned char * serial_pkt;	// Buffer do pacote.
	unsigned char * p;
	size_t header_size;
	
	/* Calcular o tamanho do cabeçalho. */
	
	header_size = sizeof(t_packet) - sizeof(void *);
	
	/* Alocar a memória para o pacote serializado. */
	
	*size = header_size + pkt->data_size;

	serial_pkt = p_alloc(*size);
	
	/* Copiar os dados: primeiro o cabeçalho, depois os dados. */
	
	memcpy(serial_pkt, pkt, header_size);
	p = serial_pkt + header_size;
	memcpy(p, pkt->data, pkt->data_size);
	
	/* Serialização concluída com sucesso. */
	
	return(serial_pkt);
}

/**
 * Transforma um pacote na forma serial em uma estrutura do tipo t_packet.
 * @param serial_pkt Ponteiro para o buffer do pacote serializado.
 * @param len Tamanho total do pacote.
 * @return Ponteiro para a estrutura t_packet contendo as informações do pacote.
 * @note A estrutura t_packet resultante é alocada pela própria função dinamicamente
 * e deve ser liberada pelo usuário.
 */
t_packet * deserialize_pkt(unsigned char * serial_pkt, size_t len) {
	t_packet * pkt;
	unsigned char * p;
	size_t header_size;
	
	/* Calcular o tamanho do cabeçalho. */
	
	header_size = sizeof(t_packet) - sizeof(void *);
	
	/* Alocar a memória para o pacote. */
	
	pkt = p_alloc(sizeof(t_packet));
	pkt->data = p_alloc(len - header_size);
	
	/* Copiar os dados. */
	
	memcpy(pkt, serial_pkt, header_size);
	p = serial_pkt + header_size;
	memcpy(pkt->data, p, pkt->data_size);
	
	return(pkt);
}

/**
 * Procura por um dado nó na lista de nós conhecidos da
 * topologia.
 * @param node Endereço IP do nó buscado.
 * @return 
 *  - NULL, caso o nó não seja encontrado.
 *  - Ponteiro para a estrutura t_tc_node do nó, caso contrário.
 */
t_tc_node * find_tc_node(t_ip_addr node) {

	t_tc_node * p;
	
	p = tc_list;
	
	while(p) {
		
		if (p->ip == node) return(p);
			
		p = (t_tc_node *) p->next;
	}
		
	return(NULL);
}

/**
 * Libera a memória alocada dinamicamente para a lista de nós conhecidos
 * da topologia.
 * @return Nada.
 */
void free_tc_list(void) {
	t_tc_node * p;
	
	while(tc_list) {
		
		p = tc_list;
		
		tc_list = (t_tc_node *) p->next;
		
		p_free(p);
	}
}

/**
 * Varre a lista de nós conhecidos da topologia atualizando seus tempos de
 * expiração. Remove nós expirados.
 * @return Nada.
 */
void update_tc_list(void) {
	
	t_tc_node * p, * prev = NULL;
	
	p = tc_list;

	while(p) {	
		
		/* Tempo expirou? */
		
		if (p->count == 1) {
			
			/* Retirar seus enlaces do grafo. */
			
			rm_from_graph(p->ip);

			if (prev != NULL) {
				prev->next = p->next;
				p_free(p);
				p = (t_tc_node *) prev->next;
			}
			else {
				tc_list = (t_tc_node *) p->next;
				p_free(p);
				p = tc_list;
			}
		}
		else {

			p->count--;

			prev = p;
			p = (t_tc_node *) prev->next;
			
		}
	}


}

