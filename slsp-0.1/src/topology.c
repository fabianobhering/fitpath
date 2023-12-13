/**
 * @file
 * @author Diego Passos <dpassos@ic.uff.br>
 * @version 1.0
 *
 * @section DESCRIPTION
 *
 * Módulo que manipula a topologia conhecida.
 */


#include "topology.h"
#include "routes.h"


/****************************************************************
 *                      Variáveis Globais                       *
 ****************************************************************/

/**
 * Ponteiro para a lista de vizinhos conhecidos.
 */
static t_neigh_node * neigh_list = NULL;

/**
 * Ponteiro para uma estrutura de grafo.
 */
t_graph_node * graph = NULL;

/**
 * Tabela de roteamento atualmente computada.
 */
int table;

/****************************************************************
 *                        Implementações                        *
 ****************************************************************/

/**
 * Libera a memória associada a uma janela de hellos de um vizinho.
 * @param lq_win Ponteiro para a janela.
 * @return Nada.
 */
void lq_window_free(t_window * lq_win) {

	t_ttl * p;

	p = lq_win->next;
	while(p) {

		lq_win->next = p->next;
		p_free(p);
		p = lq_win->next;
	}
}

/**
 * Normaliza o estado de uma janela, removendo entradas muito antigas ou excedentes.
 * @param lq_win Ponteiro para a janela.
 * @return Nada.
 */
void lq_window_normalize(t_window * lq_win) {

	t_ttl * p;
	unsigned short remaining_time;

	/*
	 * Remove o pacote mais antigo da janela.
	 */

	p = lq_win->next;

	/* 
	 * Antes de tirarmos o pacote, vamos verificar a qtd de pulsos 
	 * restantes.
	 */

	remaining_time = p->ttl;

	/* 
	 * Podemos proceder com a remocao.
	 */

	lq_win->next = p->next;
	if (lq_win->next == NULL) lq_win->last = NULL;
	p_free(p);
	lq_win->count--;

	/* 
	 * Temos que aumentar o ttl do novo 'next'.
	 */

	lq_win->next->ttl += remaining_time;

	/*
	 * Se um pacote expirou, temos um novo valor para nlq_prob.
	 */

	lq_win->nlq_prob -= aging;

#ifdef DEBUG
	if (lq_win->next) printf("\t- Novo ttl do pacote mais antigo eh %d (%s)\n", lq_win->next->ttl, __func__);
	else printf("Janela vazia!!!\n");
#endif
}

/**
 * Adiciona um hello recém recebido à janela do vizinho.
 * @param lq_win Ponteiro para a janela.
 * @return Nada.
 */
void lq_window_new_hello(t_window * lq_win) {

	t_ttl * new_ttl;

	/*
	 * Adicionar um novo ttl na janela.
	 */

	new_ttl = p_alloc(sizeof(t_ttl));
	new_ttl->ttl = lq_win->since_last;
	new_ttl->next = NULL;
	
	if (lq_win->last == NULL) {
		
		new_ttl->ttl = hello_life_time;
		lq_win->next = new_ttl;
		lq_win->last = new_ttl;
	}
	else {

		lq_win->last->next = new_ttl;
		lq_win->last = new_ttl;
	}

	/*
	 * O ultimo hello acabou de chegar.
	 */

	lq_win->since_last = 0;

	lq_win->count++;
	
	/*
	 * Calcular o novo custo.
	 */

	lq_win->nlq_prob += aging;

	if (lq_win->count > 1.0/aging + 0.001) lq_window_normalize(lq_win);

}

/**
 * Executa um timeout na janela de um vizinho. Verifica se existem 
 * informações antigas a serem removidas.
 * @param lq_win Ponteiro para a janela.
 * @return Nada.
 */
void lq_window_timeout(t_window * lq_win) {

	t_ttl * p;

	/* 
	 * Incrementar o since_last.
	 */

	lq_win->since_last++;

	/*
	 * Decrementar o tempo de vida do pacote mais velho.
	 */

	if (lq_win->next != NULL) {

		lq_win->next->ttl--;
		if (lq_win->next->ttl == 0) {

			/* 
			 * Pacote expirou. Retirar da lista.
			 */

			p = lq_win->next;
			lq_win->next = p->next;
			if (lq_win->next == NULL) lq_win->last = NULL;
			p_free(p);
			lq_win->count--;

			/*
			 * Se um pacote expirou, temos um novo valor para nlq_prob.
			 */

			lq_win->nlq_prob -= aging;
			if (lq_win->nlq_prob < 0.0) lq_win->nlq_prob = 0.0;
		}

#ifdef DEBUG
		if (lq_win->next) printf("\t- Novo ttl do pacote mais antigo eh %d (%s)\n", lq_win->next->ttl, __func__);
		else printf("Janela vazia!!!\n");
#endif
	}
}

/**
 * Adiciona um novo nó à lista de vizinhos. Verifica se o vizinho já era
 * conhecido.
 * @param neigh Endereço IP do vizinho.
 * @param rate Taxa de transmissão na qual o vizinho foi reconhecido.
 * @return 0, se a topologia não mudou. 1, caso contrário.
 */
unsigned char add_neigh(t_ip_addr neigh, unsigned char rate) {

	t_neigh_node * new_node;
	
	/* Ganhar controle sobre a estrutura de dados. */
	
	sem_wait(&neigh_mutex);
	
	/* Se o vizinho já está na lista, apenas reiniciamos o seu tempo de vida. */
	
	if ((new_node = find_neigh(neigh)) != NULL) {

		new_node->count = hello_life_time;
		lq_window_new_hello(& (new_node->lq_window[rate]));
		
		/* Liberar o controle da estrutura de dados. */
		
		sem_post(&neigh_mutex);
		
		/* Topologia não mudou. */
		
		return(0);
	}
	
	/* Senão, adicionamos um novo nó. */
	
	else {
		new_node = p_alloc(sizeof(t_neigh_node));
		new_node->ip = neigh;

		new_node->count = hello_life_time;
		new_node->cost[0] = 1.0;
		new_node->cost[1] = 1.0;
		new_node->cost[2] = 1.0;
		new_node->cost[3] = 1.0;

		/* 
		 * Inicializacao da janela de pacotes de hello.
		 */

		bzero((void *) new_node->lq_window, 4 * sizeof(t_window));
		new_node->lq_window[0].since_last = hello_life_time;
		new_node->lq_window[1].since_last = hello_life_time;
		new_node->lq_window[2].since_last = hello_life_time;
		new_node->lq_window[3].since_last = hello_life_time;
		lq_window_new_hello(& (new_node->lq_window[rate]));

		new_node->simmetric = 0;
		
		if (neigh_list == NULL) new_node->next = NULL;
		else new_node->next = (struct t_neigh_node *) neigh_list;
		neigh_list = new_node;
		
		/* Liberar o controle da estrutura de dados. */
		
		sem_post(&neigh_mutex);

		/* Topologia mudou. */
		
		return(1);
	}
}

/**
 * Dada uma lista de vizinhos reportada por um nó (através de um hello),
 * verifica se o vizinho é simétrico (ou seja, se o nó local está identificado
 * como vizinho do remetente) e guarda os valores de qualidade de enlace 
 * reportados.
 * @param neigh Endereço IP do vizinho remetente.
 * @param buffer Buffer contendo a lista de enlaces enviados pelo vizinho.
 * @return 0, se não há simetria (não conhecemos o vizinho). 1, caso contrário.
 */
unsigned char check_simmetry_and_read_lq(t_ip_addr neigh, void * buffer) {

	t_neigh_node * p;
	t_hello_entry * links;
	unsigned char is_neigh;
	unsigned char ret;
	int i;

	is_neigh = 0;
	i = 0;
	links = (t_hello_entry *) buffer;

	while (links[i].ip != 0xFFFFFFFF) {
	
		if (links[i].ip == ifce_ipaddr) {

			is_neigh = 1;
			break ;
		}
		i++;
	}
	
	sem_wait(&neigh_mutex);

	p = find_neigh(neigh);
	if (p->simmetric) {

		if (is_neigh) {

			ret = 0;
		}
		else {

			p->simmetric = 0;
			ret = 1;
		}
	}
	else {

		if (is_neigh) {

			p->simmetric = 1;
			ret = 1;
		}
		else {
			
			ret = 0;
		}
	}

	/* 
	 * Atualizar o valor de lq_prob na janela.
	 */

	p->lq_window[0].lq_prob = links[i].lq[0];
	p->lq_window[1].lq_prob = links[i].lq[1];
	p->lq_window[2].lq_prob = links[i].lq[2];
	p->lq_window[3].lq_prob = links[i].lq[3];
	
	sem_post(&neigh_mutex);

	return(ret);
}


/** 
 * Decrementa o tempo de vida de todos os vizinhos da lista, e
 * retira aqueles que tiveram seu tempo expirado.
 * @return 0, se a topologia permaneceu a mesma. 1, caso contrário.
 */
unsigned char update_neigh(void) {
	t_neigh_node * p, * prev = NULL;
	unsigned char ret_val = 0;
	
	/* Ganhar controle sobre a estrutura de dados. */
	
	sem_wait(&neigh_mutex);
	
	p = neigh_list;

	while(p) {	
		
		/* Tempo expirou? */
		
		if (p->count == 1) {
			
			lq_window_free(& (p->lq_window[0]));
			lq_window_free(& (p->lq_window[1]));
			lq_window_free(& (p->lq_window[2]));
			lq_window_free(& (p->lq_window[3]));

			if (lq_level > 2)
				net_set_ifce_rate(-1, p->ip);
			if (prev != NULL) {
				prev->next = p->next;
				p_free(p);
				p = (t_neigh_node *) prev->next;
			}
			else {
				neigh_list = (t_neigh_node *)  p->next;
				p_free(p);
				p = neigh_list;
			}

			/* A topologia mudou. Retiramos um nó. */
			
			ret_val = 1;
		}
		else {

			p->count--;

			/*
			 * Atualizar a janela de pacotes.
			 */

#ifdef DEBUG
			printf("Tirando um ttl das janelas do vizinho %s.\n", inetaddr(p->ip));
#endif

			lq_window_timeout(& (p->lq_window[0]));
			lq_window_timeout(& (p->lq_window[1]));
			lq_window_timeout(& (p->lq_window[2]));
			lq_window_timeout(& (p->lq_window[3]));

			prev = p;
			p = (t_neigh_node *) prev->next;
		}
	}

	/* Liberar o controle da estrutura de dados. */
	
	sem_post(&neigh_mutex);

	return(ret_val);
}
	
/**
 * Libera a memória dinamicamente alocada para a lista de vizinhos.
 * @return Nada.
 */
void free_neigh(void) {

	t_neigh_node * p, * prev;
	
	p = neigh_list;

	while(p) {	
		prev = p;
		p = (t_neigh_node *) p->next;
		lq_window_free(& (prev->lq_window[0]));
		lq_window_free(& (prev->lq_window[1]));
		lq_window_free(& (prev->lq_window[2]));
		lq_window_free(& (prev->lq_window[3]));
		p_free(prev);
	}
}

/**
 * Procura por um endereço na lista de vizinhos. 
 * @param neigh Endereço IP do vizinho procurado.
 * @return Um ponteiro para o nó de 'neigh' na lista, ou NULL 
 * caso não exista. 
 */
t_neigh_node * find_neigh(t_ip_addr neigh) {

	t_neigh_node * p;
	
	p = neigh_list;
	
	while(p) {

		if (p->ip == neigh) {
			return(p);
		}
		
		p = (t_neigh_node *) p->next;
	}
		
	return(NULL);
}

/**
 * Calcula a métrica (de acordo com a opção lq_level) para todos os vizinhos conhecidos.
 * @return Nada.
 * @note A função afeta a estrutura de dados da lista de vizinhos, armazenando os valores
 * calculados.
 */
void compute_metric_for_neighs() {

	t_neigh_node * p;
	int i;

	p = neigh_list;
	while(p) {

		switch(lq_level) {

			case 0:
				
				/*
				 * Hop Count.
				 */

				/*
				 * Neste caso, o custo de todo enlace (existente) eh fixo em 1.
				 * No entanto, nao precisamos setar este valor, pois isso ja eh 
				 * feito na inicializacao de um vizinho.
				 */

				break ;
			case 1:

				/* 
				 * ETX.
				 */

				/*
				 * Para o ETX, o calculo eh simples: vamos multiplicar os valores 
				 * de lq_prob e nlq_prob da janela zero. O ETX eh o reciproco desta 
				 * grandeza. 
				 */

				if (p->lq_window[0].lq_prob * p->lq_window[0].nlq_prob == 0) p->cost[0] = INFINITY;
				else p->cost[0] = 1.0/(p->lq_window[0].lq_prob * p->lq_window[0].nlq_prob); 

				break ;

			case 2:

				/*
				 * ML.
				 */

				/* 
				 * Para o ML, o procedimento eh similar. No entanto, nao calculamos
				 * o reciproco.
				 */

				p->cost[0] = (p->lq_window[0].lq_prob * p->lq_window[0].nlq_prob); 

				break ;
			case 3:

				/*
				 * MARA.
				 */

				p->cost[0] = mara_calc_cost(p->lq_window[0].lq_prob, p->lq_window[0].nlq_prob,
							    p->lq_window[1].lq_prob, p->lq_window[1].nlq_prob,
					       		    p->lq_window[2].lq_prob, p->lq_window[2].nlq_prob,
							    p->lq_window[3].lq_prob, p->lq_window[3].nlq_prob,
							    1500, p->ip);

				break ;
			case 4:

				/* 
				 * MARA-P
				 */

				p->cost[0] = marap_calc_cost(p->lq_window[0].lq_prob, p->lq_window[0].nlq_prob,
						             p->lq_window[1].lq_prob, p->lq_window[1].nlq_prob,
							     p->lq_window[2].lq_prob, p->lq_window[2].nlq_prob,
							     p->lq_window[3].lq_prob, p->lq_window[3].nlq_prob,
							     1500, p->ip);

				break ;
			case 5:

				/*
				 * MARA-RP.
				 */

				for (i = 0 ; i < nRateClasses; i++) {

					p->cost[i] = mararp_calc_cost(p->lq_window[0].lq_prob, p->lq_window[0].nlq_prob,
							           p->lq_window[1].lq_prob, p->lq_window[1].nlq_prob,
								   p->lq_window[2].lq_prob, p->lq_window[2].nlq_prob,
								   p->lq_window[3].lq_prob, p->lq_window[3].nlq_prob,
								   1500, i, p->ip);
				}


				break ;
		}

#ifdef DEBUG
fprintf(stderr, "%-17s - %.3f/%.3f %.3f/%.3f %.3f/%.3f %.3f/%.3f - %.3f/%.3f/%.3f/%.3f\n", 
			inetaddr(p->ip), 
			p->lq_window[0].lq_prob,
			p->lq_window[0].nlq_prob,
			p->lq_window[1].lq_prob,
			p->lq_window[1].nlq_prob,
			p->lq_window[2].lq_prob,
			p->lq_window[2].nlq_prob,
			p->lq_window[3].lq_prob,
			p->lq_window[3].nlq_prob,
			p->cost[0],
			p->cost[1],
			p->cost[2],
			p->cost[3]);
#endif
		p = (t_neigh_node *) p->next;
	}
}

/**
 * Gera a lista de enlaces (e seus custos) para envio em um pacote de hello.
 * @param buffer Buffer previamente alocado para receber a lista.
 * @return Nada.
 * @note O final dos dados é marcado por um enlace com IP 255.255.255.255.
 */
void gen_hello_data(void * buffer) {

	t_neigh_node * p;
	t_hello_entry * my_links;
	int i;
	
	/*
	 * TODO: verificar se o numero de vizinhos ultrapassa o tamanho do pacote.
	 */

	my_links = (t_hello_entry *) buffer;
	i = 0;

	/* Ganhar controle sobre a estrutura de dados. */
	
	sem_wait(&neigh_mutex);

	p = neigh_list;
	
	while(p) {
		
		/* Preencher os dados. */
		
		my_links[i].ip = p->ip;
		my_links[i].lq[0] = p->lq_window[0].nlq_prob;
		my_links[i].lq[1] = p->lq_window[1].nlq_prob;
		my_links[i].lq[2] = p->lq_window[2].nlq_prob;
		my_links[i].lq[3] = p->lq_window[3].nlq_prob;
		
		/* Avançar os ponteiros. */
		
		p = (t_neigh_node *) p->next;
		i++;
	}

	my_links[i].ip = 0xFFFFFFFF;

	sem_post(&neigh_mutex);
}

/**
 * Gera a lista de vizinhos (e seus custos) para envio em um pacote de topologia.
 * @param data_size Parâmetro de saída que informa o tamanho dos dados do pacote gerado.
 * @return Ponteiro para os dados gerados.
 * @note O final dos dados é marcado por um enlace com IP 255.255.255.255. A função aloca
 * o buffer para o pacote enviado. Ele deve ser liberado pelo usuário.
 */
void * gen_neigh_data(unsigned long * data_size) {
	
	t_link_info		* my_links;
	t_HNA_info		* my_HNA;
	t_neigh_node		* p;
	t_ip_list		* q;
	int 			  i;
	
	/* Ganhar controle sobre a estrutura de dados. */

	sem_wait(&neigh_mutex);

	/* Calcular os custos de todos os vizinhos. */

	compute_metric_for_neighs();

	i = 1;
	p = neigh_list;
	
	/* Varrer a lista duas vezes: uma para contar os elementos,
	 * e outra para colocá-los na área de dados. */
	
	while(p) {

		if (p->simmetric) i++;
		p = (t_neigh_node *) p->next;
	}
	
	/* Já sabemos o tamanho dos dados. */
	
	*data_size = (i + 1) * sizeof(t_link_info) + sub_nets->number_of_entries * sizeof(t_HNA_info);
	my_links = p_alloc(*data_size);
	
	/* O primeiro elemento do vetor é o ip do próprio nó. */
	
	my_links[0].ip = ifce_ipaddr;
	
	/* A partir daí, passamos para os próximos. */
	
	i = 1;
	p = neigh_list;
	
	while(p) {
		
		if (p->simmetric) {
			
			/* Preencher os dados. */
		
			my_links[i].ip = p->ip;
		
			memcpy(my_links[i].cost, p->cost, 4 * sizeof(float));
		
			i++;
		}
		/* Avançar o ponteiro. */
		
		p = (t_neigh_node *) p->next;
	}

	/*
	 * Marcador de finalizacao dos links. Inicio do HNA.
	 */

	my_links[i++].ip = 0xFFFFFFFF;
	my_HNA = (t_HNA_info *) & (my_links[i]);

	/*
	 * Varrer as sub-redes exportadas.
	 */

	q = sub_nets;
	i = 0;

	while(q) {

		my_HNA[i].ip = q->ip;
		my_HNA[i++].mask = q->mask;

		/* Avançar o ponteiro. */

		q = (t_ip_list *) q->next;
	}

	/* Liberar o controle da estrutura de dados. */
	
	sem_post(&neigh_mutex);
	
	return((void *) my_links);
}

/**
 * Adiciona as informações de um pacote de topologia recebido à estrutura
 * de dados do grafo da topologia.
 * @param data Ponteiro para a lista de enlaces recebidos.
 * @param data_size Tamanho dos dados contidos na lista de enlaces.
 * @param new Informa se o nó do qual recebemos as informações é novo,
 * ou se já o conhecíamos antes.
 * @return Nada.
 */
void add_to_graph(t_link_info * data, size_t data_size, unsigned char new) {
	
	t_graph_node * node; 
	t_link_info * q;
	t_ip_list * p;
	int i;
	size_t copy_size;
	
	
	/* Se é um novo nó, temos que criá-lo no grafo. */
	
	if (new) {
		
		node = p_alloc(sizeof(t_graph_node));
		
		/* A primeira posição do vetor guarda o ip do nó de origem do pacote. */
		
		node->ip = data[0].ip;
		
		/* Inserir o novo nó no grafo. */
		
		sem_wait(&topo_mutex);

		if (graph == NULL) node->next = NULL;
		else node->next = graph;

		graph = node;
		
	}
	else {
		
		/* Se já existe, vamos encontrá-lo. */
		
		node = graph;
		
		while (node) {
			
			if (data[0].ip == node->ip) break;
				
			node = (t_graph_node *) node->next;
		}
		
		if (!node)
			p_error(info_mismatch, "add_to_graph");
		
		sem_wait(&topo_mutex);

		/* Se não possuímos, vamos atualizar. */
		p_free(node->links);
		p_free(node->HNA);
	}

	/* Descobrir quantos enlaces estao reportados no pacote (o tamanho da area de dados a ser copiada). */

	q = & data[1]; /* Pulamos o primeiro registro (no de origem da mensagem). */
	copy_size = 0;

	while(q->ip != 0xFFFFFFFF) {
	
		copy_size += sizeof(t_link_info);
		q++;
	}

	/* Adicionar enlaces ao nó. */
	
	node->links = (t_link_info *) p_alloc(copy_size);
	memcpy(node->links, &data[1], copy_size);
	node->n_links = copy_size/sizeof(t_link_info);

	/* Adicionar o HNA ao nó. */
	
	copy_size = data_size - copy_size - 2 * sizeof(t_link_info);
	node->HNA = (t_HNA_info *) p_alloc(copy_size);
	q++;
	memcpy(node->HNA, q, copy_size);
	node->n_HNAs = copy_size/sizeof(t_HNA_info);

	/* Remover as entradas inuteis (que ja temos no nosso HNA). */

	for (i = 0; i < node->n_HNAs; i++) {

		/* Procurar a entrada na nossa propria lista de HNAs. */
		
		p = sub_nets;
		while(p) {

			if (p->ip == node->HNA[i].ip && p->mask == node->HNA[i].mask) break ;
			p = p->next;
		}

		if (p) {

			/* Encontrada. Vamos retirar da lista de HNA's. */

			node->n_HNAs--;
			node->HNA[node->n_HNAs] = node->HNA[i];
		}
	}

	sem_post(&topo_mutex);
	
#if 0
	p = graph;
	if (!daemonize) {

		fprintf(stderr, "****************************************\nTopologia atual:\n\n");
		
		while(p) {
			
			for (i = 0; i < p->n_links; i++) {
				fprintf(stderr, "%s -> %s [metrica=%.3f/%.3f/%.3f/%.3f]\n", p->ip, p->links[i].ip, 
					p->links[i].cost[0],
					p->links[i].cost[1],
					p->links[i].cost[2],
					p->links[i].cost[3]);
			}
			fprintf(stderr, "\n");
		
			p = (t_graph_node *) p->next;		
		}
			
		fprintf(stderr, "Fim da topologia\n****************************************\n\n");

	}
#endif	
	if (lq_level == 5) {

		for (i = 0; i < nRateClasses; i++) {

			table = i;
			dijkstra();
		}
	}
	else {
		
		table = RT_TABLE_MAIN - 1; // A funcao update_routes soma 1
		dijkstra();
	}
}

/**
 * Remove um nó do grafo.
 * @param ip Endereço IP do nó a ser removido.
 * @return Nada.
 */
void rm_from_graph(t_ip_addr ip) {
	
	t_graph_node * p, * prev = NULL;
	
	p = graph;
	
	while(p) {
		
		if (p->ip == ip) {
			
			if (prev == NULL) graph = (t_graph_node *) p->next;
			else prev->next = p->next;
				
			p_free(p->links);
			p_free(p->HNA);
			p_free(p);
			
			return ;
		}
		
		prev = p;
		
		p = (t_graph_node *) p->next;
	}
	
	p_warning(no_graph_node, "rm_from_graph");
}

/**
 * Retorna a posição de um nó no grafo da topologia.
 * @param ip Endereço IP do nó buscado.
 * @return Posição do nó na lista de topologia. Caso não seja encontrado, 
 * retorna -1.
 */
unsigned int find_graph_node_pos(t_ip_addr ip) {
	
	unsigned int i = 0;
	t_graph_node * p;
	
	p = graph;
	
	while(p) {
		if (ip == p->ip) return(i);
		p = (t_graph_node *) p->next;
		i++;
	}
	
	return(-1);
}

/**
 * Libera a memória alocada dinamicamente para o grafo da topologia.
 * @return Nada.
 */
void free_graph(void) {
	 t_graph_node * p;
	
	
	while(graph) {
		
		p = graph;
	
		p_free(p->links);
		p_free(p->HNA);
		
		graph = (t_graph_node *) p->next;
		
		p_free(p);

	}		
}

/**
 * Retorna o valor inicial de um caminho em cada métrica.
 * @return Custo inicial.
 */
inline float metric_initial_cost() {

	switch(lq_level) {
		
		case 0:
		case 1:
		case 3:
		case 4:
		case 5:

			/* 
			 * Hop Count, ETX, MARA, MARA-P, MARA-RP.
			 */

			return(0.0);
		case 2:

			/* 
			 * ML.
			 */

			return(1.0);
	}

	/*
	 * Para evitar o warning do compilador.
	 */

	return(0.0);
}

/**
 * Computa o custo resultante de um caminho, dado o custo até o penúltimo nó e o 
 * custo do enlace entre o penúltimo e o último nó, de acordo com a métrica selecionada.
 * @param path_cost Custo do caminho sem o último enlace.
 * @param link_cost Custo do último enlace.
 * @return Valor do custo do caminho completo.
 */
inline float metric_composite_function(float path_cost, float link_cost) {

	float new_cost;

	switch(lq_level) {

		case 0:
		case 1:
		case 3:
		case 4:
		case 5:

			/* 
			 * Hop Count, ETX, MARA, MARA-P, MARA-RP utilizam a soma
			 * como funcao de composicao.
			 */

			new_cost = path_cost + link_cost;
			break ;

		case 2:

			/*
			 * ML utiliza a multiplicacao.
			 */

			if (link_cost == INFINITY) new_cost = INFINITY;
			else new_cost = path_cost * link_cost;
			break ;
	}

	return(new_cost);
}

/**
 * Dados os custos de dois caminhos, verifica se o custo do do segundo é
 * melhor que o custo do primeiro.
 * @param current_dist Custo do melhor caminho conhecido atualmente.
 * @param possible_dist Custo do caminho alternativo.
 * @return 1, se caminho alternativo é melhor. 0, caso contrário.
 */
inline int metric_is_better(float current_dist, float possible_dist) {

	switch(lq_level) {

		case 0:
		case 1:
		case 3:
		case 4:
		case 5:

			/* 
			 * No Hop Count, ETX, MARA, MARA-P, MARA-RP, quanto menor
			 * o valor, melhor.
			 */

			if (possible_dist < current_dist) return(1);
			else return(0);

		case 2:

			/*
			 * ML utiliza a logica inversa.
			 */

			if (possible_dist == INFINITY) return(0);
			if (current_dist == INFINITY) return(1);
			if (possible_dist > current_dist) return(1);
			else return(0);
	}

	/*
	 * Apenas para evitar o warning do compilador.
	 */

	return(0);

}

/**
 * Calcula os melhores caminhos do nó local para todos os demais nós da rede,
 * utilizando o algoritmo de dijkstra.
 * @return Nada.
 * @note Armazena os resultados em uma estrutura interna de tabela de roteamento
 * e chama o módulo de manipulação das tabela do sistema para atualizar.
 */
void dijkstra(void) {

	t_graph_node	* p, * q, * def, * remain, * best_src, * HNA_list;
	unsigned char has_root = 0;
	unsigned char best_hops = 0;
	float best_dist, possible_dist;
	t_ip_addr best_dest, best_first;
	int i;

	/* Vamos preencher as listas auxiliares. */

	def = NULL;
	remain = NULL;

	/* Primeiro, a lista de nos restantes. */

	p = graph;

	while(p) {
		
		if (p->ip == ifce_ipaddr) {

			has_root = 1;
			p->path_cost = metric_initial_cost();
			p->path_hops = 0;
			p->next_hop = 0;
			append_tmp_graph_node(p, & def);
		}
		else {

			append_tmp_graph_node(p, & remain);
		}

		p = (t_graph_node *) p->next;
	}
	
	if (!has_root) {

		/* Liberar a memória. */
	
		free_tmp_graph(remain);
		free_tmp_graph(def);

		return ;
	}

	/*
	 * Comecar o processamento.
	 */
#ifdef DEBUG
printf("Iniciando o Dijkstra\n");
#endif

	while(remain) {

#ifdef DEBUG
printf("Nova iteracao do dijkstra");
#endif

		/*
		 * Encontrar o no mais proximo de qq definitivo.
		 */
	
		best_dist = INFINITY;
		best_dest = 0;
		best_src = NULL;
		best_first = 0;
		best_hops = 0;
		p = def;
		while(p) {

#ifdef DEBUG
printf("Analisando os links do no %s\n", inetaddr(p->ip));
#endif

			for (i = 0 ; i < p->n_links; i++) {
			
				/*
				 * Utilizamos uma funcao auxiliar para computar a funcao
				 * de composicao de cada metrica. Isto permite a funcao dijkstra 
				 * ser o mais generica possivel.
				 */

				if (table == RT_TABLE_MAIN - 1) {

					if (p->links[i].cost[0] == INFINITY) continue ;
					possible_dist = metric_composite_function(p->path_cost, p->links[i].cost[0]);
				}
				else {

					if (p->links[i].cost[table] == INFINITY) continue ;
					possible_dist = metric_composite_function(p->path_cost, p->links[i].cost[table]);
				}

				/*
				 * Novamente, utilizamos uma funcao auxiliar para a comparacao
				 * entre custos. Para decidir empates, vamos utilizar o numero de
				 * saltos (que tambem pode gerar empates).
				 */
#ifdef DEBUG
printf("\t- Analisando destino %s com possible metric %f e possible saltos %d contra best metric %f e best hops %d\n", inetaddr(p->links[i].ip), possible_dist, p->path_hops, best_dist, best_hops - 1);
printf("\t- %08X contra %08X\n", * ((unsigned int *) & possible_dist), * ((unsigned int *) & best_dist));
printf("\t\t- best_dist == possible_dist: %d e best_hops - 1 > p->path_hops: %d\n", FLOAT_IS_EQUAL(best_dist, possible_dist), best_hops - 1 > p->path_hops);
#endif

				if (metric_is_better(best_dist, possible_dist) ||
				   (FLOAT_IS_EQUAL(best_dist, possible_dist) && best_hops - 1 > p->path_hops)) {
#ifdef DEBUG
printf("\t\t- Melhor\n");
#endif
					if (find_tmp_graph_node(p->links[i].ip, def)) continue ;
#ifdef DEBUG
printf("\t\t- Nao eh definitivo\n");
#endif

					best_dist = possible_dist;
					best_dest = p->links[i].ip;
					best_src = p;
					best_hops = p->path_hops + 1;

					/*
					 * Decidir quem eh o primeiro salto.
					 */

					if (p->path_hops == 0) best_first = 0;
					else if (p->path_hops == 1) best_first = p->ip;
					else best_first = p->next_hop;
				}
			}
			p = p->next;
		}

		/*
		 * Teste de sanidade.
		 */

		if (best_dest == 0) break;

		/*
		 * Mover o no selecionado do conjunto remain para o conjunto def.
		 */

		p = remove_tmp_graph_node(best_dest, & remain);

		/* 
		 * Nao ha links do no de destino para nenhum outro no.
		 * Logo, ele nao esta na lista de remain.
		 */

		if (p == NULL) {
			
			p = p_alloc(sizeof(t_graph_node));
			p->ip = best_dest;
			p->n_links = 0;
		}

		p->path_cost = best_dist;
		p->path_hops = best_hops;
		p->next_hop = best_first;
		append_tmp_graph_node(p, & def);
		p_free(p);
	}
#if 0
	if (!daemonize) {
		
		printf("################################################\n");
		printf("#            Resulting Routes                  #\n");
		printf("################################################\n");
		p = def;
		while(p) {

			printf("%-15s %-15s - %f\n", p->ip, p->next_hop, p->path_cost);
			p = p->next;
		}
	}
#endif

	/*
	 * Adicionar as rotas para os demais nos da rede.
	 */

	update_routes(ifce_ipaddr, def, table);
	if (remain != NULL) {

		if (!daemonize)
			printf("%s: there are unreacheable nodes on the graph!!!\n", __func__);
	}

	/* Liberar a memória da lista de restantes. */
	
	free_tmp_graph(remain);

	if (def == NULL) return ;

	/*
	 * Ultimo passo: fazer a lista de sub-redes a serem adicionadas.
	 * Vamos varrer a lista de definitivos de tras para frente (ordenada por custo).
	 */

	remain = def;
	def = NULL;
	while(remain) {

		p = remain->next;
		remain->next = def;
		def = remain;
		remain = p;
	}

	/*
	 * A lista agora esta ordenada de maneira crescente. O primeiro elemento eh
	 * o proprio no corrente. Obviamente, nao precisamos das sub-redes exportadas
	 * por ele. Vamos ignora-lo.
	 */

	p = def->next;
	HNA_list = NULL;

	while(p) {

		/*
		 * Varrer todos os HNA's do no.
		 */

		for (i = 0; i < p->n_HNAs; i++) {

			/*
			 * Verificar se o HNA ja foi inserido anteriormente.
			 */

			q = HNA_list;
			while(q) {

				if (q->ip == p->HNA[i].ip && q->mask == p->HNA[i].mask) break ;
				q = q->next;
			}

			/*
			 * Se a entrada foi encontrada, passamos para o proximo registro.
			 */

			if (q) continue ;

			/* 
			 * Senao, adicionamos um novo no.
			 */

			q = (t_graph_node *) p_alloc(sizeof(t_graph_node));

			q->ip = p->HNA[i].ip;
			q->mask = p->HNA[i].mask;
			if (p->next_hop == 0)
				q->next_hop = p->ip;
			else
				q->next_hop = p->next_hop;
			q->path_cost = p->path_cost;
			q->path_hops = p->path_hops;

			q->next = HNA_list;
			HNA_list = q;
		}

		p = p->next;
	}

	/*
	 * Atualizar as rotas para as sub-redes.
	 */

	update_subnet_routes(ifce_ipaddr, HNA_list, table);

	/* Liberar o restante da memoria. */
		
	free_tmp_graph(def);

	while(HNA_list) {

		p = HNA_list->next;
		p_free(HNA_list);
		HNA_list = p;
	}
}

/**
 * Adiciona um novo nó ao final de uma lista temporária de nós do grafo.
 * @param p Nó a ser adicionado.
 * @param list Lista temporária.
 * @return Nada.
 */
void append_tmp_graph_node(t_graph_node * p, t_graph_node ** list) {

	t_graph_node * copy;

	copy = p_alloc(sizeof(t_graph_node));
	* copy = * p;

	copy->next = * list;
	* list = copy;
}

/**
 * Libera a memória alocada dinamicamente para a lista temporária de 
 * nós da topologia.
 * @param list Ponteiro para a lista.
 * @return Nada.
 */
void free_tmp_graph(t_graph_node * list) {

	t_graph_node * p;

	while (list) {

		p = list;
		list = list->next;
		p_free(p);
	}
}

/**
 * Procura por um nó específico na lista temporária de nós da rede.
 * @param node Endereço IP do nó buscado.
 * @param list Lista temporária de nós.
 * @return Ponteiro para o nó na lista. NULL, se nó não for encontrado.
 */
t_graph_node * find_tmp_graph_node(t_ip_addr node, t_graph_node * list) {

	while(list) {

		if (list->ip == node) return(list);
		list = list->next;
	}

	return(NULL);
}

/**
 * Remove um nó de uma lista temporária de npós da rede.
 * @param node Endereço IP do nó a ser removido.
 * @param list Ponteiro para a lista.
 * @return Retorna um ponteiro para o nó removido.
 */
t_graph_node * remove_tmp_graph_node(t_ip_addr node, t_graph_node ** list) {

	t_graph_node * p, * ret;

	/*
	 * Verificar se eh o primeiro elemento.
	 */

	if ((* list)->ip == node) {

		p = * list;
		* list = (* list)->next;
		return(p);
	}

	/*
	 * Eh outro elemento.
	 */

	p = * list;

	while(p->next) {

		if (p->next->ip == node) break ;
		p = p->next;
	}

	if (p->next == NULL) return(NULL);	/* Nao encontramos. */

	/*
	 * Vamos retirar o elemento da lista.
	 */

	ret = p->next;
	p->next = p->next->next;

	return(ret);
}

