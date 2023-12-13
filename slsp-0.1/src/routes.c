/**
 * @file
 * @author Diego Passos <dpassos@ic.uff.br>
 * @version 1.0
 *
 * @section DESCRIPTION
 *
 * Módulo que implementa rotinas de manipulação da tabela de rotas.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <errno.h>

#include "routes.h"
#include "nl.h"
#include "defs.h"

/****************************************************************
 *                          Macros                              *
 ****************************************************************/

/*
 * Macro que transforma uma mascara no formato CIDR (# de bits) em
 * no seu formato por extenso (numeros e pontos).
 */

/**
 * Máscara para adição de rotas do tipo host.
 */
#define HOST_NETMASK	32u

/****************************************************************
 *                      Variáveis Globais                       *
 ****************************************************************/

/**
 * Lista de rotas para hosts.
 */
static t_rtentry * routes = NULL;

/**
 * Lista de rotas para sub-redes.
 */
static t_rtentry * subnet_routes = NULL;

/*
 * Codigo antigo. Não remover, por enquanto.
 */
#if 0
static int rt_fd;
#endif

/****************************************************************
 *                        Implementações                        *
 ****************************************************************/

/*
 * Codigo antigo. Não remover, por enquanto.
 */
#if 0
void init_routes() {
	
	if ((rt_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		
		p_error(socket_error, "init_routes");
	}
}
#endif

/**
 * Finaliza o módulo.
 * @return Nada.
 */
void finalize_routes() {
	
	free_routes();
	free_subnet_routes();
/*
 * Codigo antigo. Não remover, por enquanto.
 */
#if 0	
	close(rt_fd);
#endif
}

/*
 * Codigo antigo. Não remover, por enquanto.
 */

#if 0
void add_route_MARARP(t_ip_addr dest, t_ip_addr gw, unsigned char metric, t_ip_addr netmask, int table) {

	char cmd[100];

	if (gw == 0)
		sprintf(cmd, "%s route add %s dev %s table %d", ip_path, inetaddr(dest), ifce_name, table);
	else
		sprintf(cmd, "%s route add %s via %s table %d", ip_path, inetaddr(dest), inetaddr(gw), table);

	system(cmd);
}

void del_route_MARARP(t_ip_addr dest, t_ip_addr gw, unsigned char metric, t_ip_addr netmask, int table) {

	char cmd[100];

	if (gw == 0)
		sprintf(cmd, "%s route del %s table %d dev %s", ip_path, inetaddr(dest), table, ifce_name);
	else
		sprintf(cmd, "%s route del %s via %s table %d", ip_path, inetaddr(dest), inetaddr(gw), table);

	system(cmd);
}
#endif

/*
 * Funcao de adicao de rotas.
 */

/**
 * Adiciona uma rota.
 * @param dest Endereço IP do nó de destino da rota.
 * @param gw Endereço IP do gateway para esta rota.
 * @param metric Valor do campo 'métrica' da rota.
 * @param mask Máscara (CIDR) da rota.
 * @param table Índice da tabela de roteamento desejada.
 * @param ifce_index índice da interface de saída da rota.
 * @return Nada.
 */
void add_route(t_ip_addr dest, t_ip_addr gw, unsigned int metric, unsigned char mask, unsigned char table, int ifce_index) {

	t_route_req req;

	/*
	 * Zerar toda a estrutura.
	 */

	memset(&req, 0, sizeof(req));

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL;
	req.n.nlmsg_type = RTM_NEWROUTE;

	req.r.rtm_family = AF_INET;
	req.r.rtm_protocol = RTPROT_BOOT;
	req.r.rtm_scope = RT_SCOPE_UNIVERSE;
	req.r.rtm_type = RTN_UNICAST;

	/*
	 * Mascara de subrede.
	 */

	req.r.rtm_dst_len = mask;
	
	/*
	 * Indice da tabela de roteamento.
	 */

	req.r.rtm_table = table;

	/*
	 * Adicionar a opcao RTA_DST (destino da rota).
	 */

	addattr_l(& (req.n), sizeof(req), RTA_DST, & dest, 4);

	/*
	 * Se existir, adicionar a opcao RTA_GATEWAY (gateway).
	 */

	if (gw) addattr_l(& (req.n), sizeof(req), RTA_GATEWAY, & gw, 4);

	/*
	 * Adicionar a opcao da interface de saida.
	 */

	addattr32(& (req.n), sizeof(req), RTA_OIF, ifce_index);

	/*
	 * Adicionar a opcao de metrica.
	 */

	addattr32(& (req.n), sizeof(req), RTA_PRIORITY, metric);

	/*
	 * Se nao temos um gateway (0.0.0.0) o escopo da rota eh do tipo link.
	 */

	if (!gw) req.r.rtm_scope = RT_SCOPE_LINK;

	/*
	 * Fazer o pedido ao kernel.
	 */

	rtnl_send_request(& (req.n));
}

/*
 * Funcao de remocao de rotas.
 */

/**
 * Remove uma rota.
 * @param dest Endereço IP do nó de destino da rota.
 * @param gw Endereço IP do gateway para esta rota.
 * @param metric Valor do campo 'métrica' da rota.
 * @param mask Máscara (CIDR) da rota.
 * @param table Índice da tabela de roteamento desejada.
 * @param ifce_index índice da interface de saída da rota.
 * @return Nada.
 */
void del_route(t_ip_addr dest, t_ip_addr gw, unsigned int metric, unsigned char mask, unsigned char table, int ifce_index) {

	t_route_req req;

	/*
	 * Zerar toda a estrutura.
	 */

	memset(&req, 0, sizeof(req));

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.n.nlmsg_type = RTM_DELROUTE;

	req.r.rtm_family = AF_INET;
	req.r.rtm_protocol = RTPROT_BOOT;
	req.r.rtm_scope = RT_SCOPE_NOWHERE;
	req.r.rtm_type = RTN_UNICAST;

	/*
	 * Mascara de subrede.
	 */

	req.r.rtm_dst_len = mask;
	
	/*
	 * Indice da tabela de roteamento.
	 */

	req.r.rtm_table = table;

	/*
	 * Adicionar a opcao RTA_DST (destino da rota).
	 */

	addattr_l(& (req.n), sizeof(req), RTA_DST, & dest, 4);

	/*
	 * Se existir, adicionar a opcao RTA_GATEWAY (gateway).
	 */

	if (gw) addattr_l(& (req.n), sizeof(req), RTA_GATEWAY, & gw, 4);

	/*
	 * Adicionar a opcao da interface de saida.
	 */

	addattr32(& (req.n), sizeof(req), RTA_OIF, ifce_index);

	/*
	 * Adicionar a opcao de metrica.
	 */

	addattr32(& (req.n), sizeof(req), RTA_PRIORITY, metric);

	/*
	 * Fazer o pedido ao kernel.
	 */

	rtnl_send_request(& (req.n));
}

/*
 * Codigo antigo. Não remover, por enquanto.
 */
#if 0
void add_route(t_ip_addr dest, t_ip_addr gw, unsigned char metric, t_ip_addr netmask) {

	struct rtentry rt;
		
	/* Limpar a estrutura. */
	
	memset((char *) &rt, 0, sizeof(struct rtentry));

	/* Configurar as flags. */
	rt.rt_flags = RTF_UP;

	if (gw) rt.rt_flags |= RTF_GATEWAY;
	if (netmask == 0xFFFFFFFF) rt.rt_flags |= RTF_HOST;

	// TODO: colocar rotas para as subredes exportadas pelos outros nós. 
	
	rt.rt_metric = metric;
	rt.rt_dev = ifce_name;

	/* Configurar os endereços da rota. */
	
	/* Destino. */
	
	((struct sockaddr_in *) &rt.rt_dst)->sin_family = AF_INET;
	((struct sockaddr_in *) &rt.rt_dst)->sin_addr.s_addr = dest;
	bzero(((struct sockaddr_in *) &rt.rt_dst)->sin_zero, 8);

	/* Gateway. */
	
	((struct sockaddr_in *) &rt.rt_gateway)->sin_family = AF_INET;
	bzero(((struct sockaddr_in *) &rt.rt_gateway)->sin_zero, 8);
	((struct sockaddr_in *) &rt.rt_gateway)->sin_addr.s_addr = gw;
	
	/* Máscara. */
	
	((struct sockaddr_in *) &rt.rt_genmask)->sin_family = AF_INET;
	((struct sockaddr_in *) &rt.rt_genmask)->sin_addr.s_addr = netmask;
	bzero(((struct sockaddr_in *) &rt.rt_genmask)->sin_zero, 8);
	
	/* Fazer a chamada de sistema. */

try_add:	
	errno = 0;
	if (ioctl(rt_fd, SIOCADDRT, &rt) < 0) {
		
		if (errno == EINTR) goto try_add;
		perror(NULL);
		p_warning(bad_route, "add_route");
	}

}

void del_route(t_ip_addr dest, t_ip_addr gw, unsigned char metric, t_ip_addr netmask) {

	struct rtentry rt;
		
	/* Limpar a estrutura. */
	
	memset((char *) &rt, 0, sizeof(struct rtentry));

	/* Configurar as flags. */
	
	rt.rt_flags = RTF_UP;

	if (gw) rt.rt_flags |= RTF_GATEWAY;
	if (netmask == 0xFFFFFFFF) rt.rt_flags |= RTF_HOST;

	// TODO: colocar rotas para as subredes exportadas pelos outros nós. 
	
	rt.rt_metric = metric;
	rt.rt_dev = ifce_name;

	/* Configurar os endereços da rota. */
	
	/* Destino. */
	
	((struct sockaddr_in *) &rt.rt_dst)->sin_family = AF_INET;
	((struct sockaddr_in *) &rt.rt_dst)->sin_addr.s_addr = dest;
	bzero(((struct sockaddr_in *) &rt.rt_dst)->sin_zero, 8);

	/* Gateway. */
	
	((struct sockaddr_in *) &rt.rt_gateway)->sin_family = AF_INET;
	bzero(((struct sockaddr_in *) &rt.rt_gateway)->sin_zero, 8);

	if (gw == dest) {
		((struct sockaddr_in *) &rt.rt_gateway)->sin_addr.s_addr = gw;
		rt.rt_flags |= RTF_GATEWAY;
	}
	else
		((struct sockaddr_in *) &rt.rt_gateway)->sin_addr.s_addr = 0;

	/* Máscara. */
	
	((struct sockaddr_in *) &rt.rt_genmask)->sin_family = AF_INET;
	((struct sockaddr_in *) &rt.rt_genmask)->sin_addr.s_addr = netmask;
	bzero(((struct sockaddr_in *) &rt.rt_genmask)->sin_zero, 8);
	
	/* Fazer a chamada de sistema. */
	
try_del:
	errno = 0;
	if (ioctl(rt_fd, SIOCDELRT, &rt) < 0) {
		
		if (errno == EINTR) goto try_del;
		p_warning(bad_route, "del_route");
	}

}

#endif

/**
 * Atualiza a tabela de roteamento com a lista de rotas resultantes da execução
 * do algoritmo de Dijkstra (apenas hosts).
 *
 * @param self Endereço IP do nó.
 * @param list Nova lista de rotas.
 * @param table Índice da tabela de roteamento a ser usada.
 * @return Nada.
 */
void update_routes(t_ip_addr self, t_graph_node * list, int table) {
	
	t_rtentry * p_route, * new_routes = NULL;
	unsigned char metric;
	
	table = table + 1;

	/* A idéia é varrer todos os nós resultantes do algoritmo de dijkstra
	 * e, para cada um deles, procurá-los na lista de rotas atuais. Três
	 * possibilidades: 
	 *   1) O nó não é encontrado: neste caso, simplesmente adicionamos a 
	 * nova rota.
	 *   2) O nó existe, porém tem informações diferentes (gateway ou métrica):
	 * removemos a rota, retiramos o nó da lista atual, e adicionamos a rota
	 * com as novas informações. 
	 *   3) O nó existe e as informações são iguais: simplesmente retiramos o
	 * nó da lista atual.
	 * Em todos os 3 casos, inserimos um nó na nova lista de rotas. Após o
	 * término da varredura, varremos o que sobrou da lista de rotas atual e 
	 * retiramos todas as rotas pendentes (se os nós sobraram na lista, então
	 * os destinos não existem mais). */
	
	while(list) {
		
		/* Não temos rota para o próprio nó. */
		
		if (list->ip == self) {
			
			list = list->next;
			continue;
		}
		
		metric = list->path_hops;

		/* Já sabemos o próximo salto até o destino. Vamos verificar a lista
		 * de rotas atuais. */
		
		if ((p_route = extract_route(list->ip, table))) {
			
			/* O destino já existe na tabela de rotas atual. Vamos verificar 
			 * se a rota se manteve. */
			
			if (p_route->gw != list->next_hop || p_route->metric != metric) {
				
				/* Segundo caso. */
				
				del_route(list->ip, p_route->gw, p_route->metric, HOST_NETMASK, table, ifce_index);
				add_route(list->ip, list->next_hop, metric, HOST_NETMASK, table, ifce_index);
				
				p_route->gw = list->next_hop;
				p_route->metric = metric;
			}
			
			/* Terceiro caso fica implicito, pois basta remover o nó da
			 * lista atual. */
				
			/* Vamos "reaproveitar" o nó da lista atual. */
			
			p_route->next = NULL;
		}
		else {
	
			/* O destino não existe. Vamos adicionar a nova rota. */
			
			add_route(list->ip, list->next_hop, metric, HOST_NETMASK, table, ifce_index);
			
			/* Temos que alocar o novo nó da lista de rotas. */
			
			p_route = p_alloc(sizeof(t_rtentry));
			
			/* E preencher seus campos. */
			
			p_route->dest = list->ip;
			p_route->gw = list->next_hop;
			p_route->metric = metric;
			p_route->table = table;
		}
		
		/* Em todos os 3 casos, um nó apontado por 'p_route' deve ser 
		 * inserido na nova lista de rotas. */
		
		if (new_routes == NULL) p_route->next = NULL;
		else p_route->next = (struct t_rtentry *) new_routes;
		new_routes = p_route;
		
		/* Próximo nó. */
		list = list->next;
	}

	/* Agora precisamos verificar se não existem rotas sobrando na lista 
	 * atual (neste ponto, já antiga). */

	while(routes) {
		
		if (routes->table != table) {
			
			/*
			 * Estas entradas nao foram afetadas pelo processamento. Vamos simplesmente
			 * copiar a entrada para a nova lista de rotas.
			 */

			p_route = routes;
			routes = (t_rtentry *) routes->next;

			if (new_routes == NULL) p_route->next = NULL;
			else p_route->next = (struct t_rtentry *) new_routes;
			new_routes = p_route;

			continue ;
		}

		del_route(routes->dest, routes->gw, routes->metric, HOST_NETMASK, routes->table, ifce_index);

		p_route = routes;
		routes = (t_rtentry *) routes->next;
		p_free(p_route);
	}
	
	/* A última coisa a ser atualizada: */
	
	routes = new_routes;
}

/**
 * Atualiza a tabela de roteamento com a lista de rotas resultantes da execução
 * do algoritmo de Dijkstra (apenas sub-redes).
 *
 * @param self Endereço IP do nó.
 * @param list Nova lista de rotas.
 * @param table Índice da tabela de roteamento a ser usada.
 * @return Nada.
 */
void update_subnet_routes(t_ip_addr self, t_graph_node * list, int table) {
	
	t_rtentry * p_route, * new_routes = NULL;
	unsigned char metric;
	
	table = table + 1;

	/* A idéia é varrer todos os nós resultantes do algoritmo de dijkstra
	 * e, para cada um deles, procurá-los na lista de rotas atuais. Três
	 * possibilidades: 
	 *   1) O nó não é encontrado: neste caso, simplesmente adicionamos a 
	 * nova rota.
	 *   2) O nó existe, porém tem informações diferentes (gateway ou métrica):
	 * removemos a rota, retiramos o nó da lista atual, e adicionamos a rota
	 * com as novas informações. 
	 *   3) O nó existe e as informações são iguais: simplesmente retiramos o
	 * nó da lista atual.
	 * Em todos os 3 casos, inserimos um nó na nova lista de rotas. Após o
	 * término da varredura, varremos o que sobrou da lista de rotas atual e 
	 * retiramos todas as rotas pendentes (se os nós sobraram na lista, então
	 * os destinos não existem mais). */
	
	while(list) {
		
		/* Não temos rota para o próprio nó. */
		
		if (list->ip == self) {
			
			list = list->next;
			continue;
		}
		
		metric = list->path_hops;

		/* Já sabemos o próximo salto até o destino. Vamos verificar a lista
		 * de rotas atuais. */
		
		if ((p_route = extract_subnet_route(list->ip, list->mask, table))) {
			
			/* O destino já existe na tabela de rotas atual. Vamos verificar 
			 * se a rota se manteve. */
			
			if (p_route->gw != list->next_hop || p_route->metric != metric) {
				
				del_route(list->ip, p_route->gw, p_route->metric, list->mask, table, ifce_index);
				add_route(list->ip, list->next_hop, metric, list->mask, table, ifce_index);
				
				p_route->gw = list->next_hop;
				p_route->metric = metric;
			}
			
			/* Terceiro caso fica implicito, pois basta remover o nó da
			 * lista atual. */
				
			/* Vamos "reaproveitar" o nó da lista atual. */
			
			p_route->next = NULL;
		}
		else {
	
			/* O destino não existe. Vamos adicionar a nova rota. */
			
			add_route(list->ip, list->next_hop, metric, list->mask, table, ifce_index);
			
			/* Temos que alocar o novo nó da lista de rotas. */
			
			p_route = p_alloc(sizeof(t_rtentry));
			
			/* E preencher seus campos. */
			
			p_route->dest = list->ip;
			p_route->mask = list->mask;
			p_route->gw = list->next_hop;
			p_route->metric = metric;
			p_route->table = table;
		}
		
		/* Em todos os 3 casos, um nó apontado por 'p_route' deve ser 
		 * inserido na nova lista de rotas. */
		
		if (new_routes == NULL) p_route->next = NULL;
		else p_route->next = (struct t_rtentry *) new_routes;
		new_routes = p_route;
		
		/* Próximo nó. */
		list = list->next;
	}

	/* Agora precisamos verificar se não existem rotas sobrando na lista 
	 * atual (neste ponto, já antiga). */

	while(subnet_routes) {
		
		if (subnet_routes->table != table) {
			
			/*
			 * Estas entradas nao foram afetadas pelo processamento. Vamos simplesmente
			 * copiar a entrada para a nova lista de rotas.
			 */

			p_route = subnet_routes;
			subnet_routes = (t_rtentry *) subnet_routes->next;

			if (new_routes == NULL) p_route->next = NULL;
			else p_route->next = (struct t_rtentry *) new_routes;
			new_routes = p_route;

			continue ;
		}

		del_route(subnet_routes->dest, subnet_routes->gw, subnet_routes->metric, subnet_routes->mask, subnet_routes->table, ifce_index);
		p_route = subnet_routes;
		subnet_routes = (t_rtentry *) subnet_routes->next;
		p_free(p_route);
	}
	
	/* A última coisa a ser atualizada: */
	
	subnet_routes = new_routes;
}

/* Procura uma rota e, caso exista, a remove da lista. */

/**
 * Procura uma rota (para host) e, caso existe, a remove da lista de rotas
 * atuais.
 * @param dest Endereço IP do nó de destino da rota.
 * @param table Índice da tabela de roteamento a ser usada.
 * @return Ponteiro para a rota recém-removida da lista.
 */
t_rtentry * extract_route(t_ip_addr dest, int table) {
	
	t_rtentry * p = routes, * prev = NULL;
	
	while(p) {
		
		if (dest == p->dest && p->table == table) {
			
			if (prev == NULL) routes = (t_rtentry *) p->next;
			else prev->next = p->next;
			
			/* Comparado com a utilização de um return, o break gera
			 * um código menor. TODO: rever outros locais em que isto 
			 * possa acontecer. */
		
			break;
		}		
		
		prev = p;
		p = (t_rtentry *) p->next;
		
	}
	
	return(p);
	
}

/* Procura uma rota (para uma sub-rede) e, caso exista, a remove da lista. */

/**
 * Procura uma rota (para sub-rede) e, caso existe, a remove da lista de rotas
 * atuais.
 * @param dest Endereço IP da sub-rede de destino da rota.
 * @param mask Máscara da sub-rede.
 * @param table Índice da tabela de roteamento a ser usada.
 * @return Ponteiro para a rota recém-removida da lista.
 */
t_rtentry * extract_subnet_route(t_ip_addr dest, unsigned char mask, int table) {
	
	t_rtentry * p = subnet_routes, * prev = NULL;
	
	while(p) {
		
		if (dest == p->dest && p->table == table && mask == p->mask) {
			
			if (prev == NULL) subnet_routes = (t_rtentry *) p->next;
			else prev->next = p->next;
			
			/* Comparado com a utilização de um return, o break gera
			 * um código menor. TODO: rever outros locais em que isto 
			 * possa acontecer. */
		
			break;
		}		
		
		prev = p;
		p = (t_rtentry *) p->next;
	}
	
	return(p);
}

/** 
 * Libera a memória alocada dinamicamente para a estrutura das rotas.
 * @return Nada.
 */
void free_routes() {
	
	t_rtentry * p_route;

	while(routes) {
		
		del_route(routes->dest, routes->gw, routes->metric, HOST_NETMASK, routes->table, ifce_index);
		p_route = routes;
		routes = (t_rtentry *) routes->next;
		p_free(p_route);
	}
}

/** 
 * Libera a memória alocada dinamicamente para a estrutura das rotas para sub-redes.
 * @return Nada.
 */
void free_subnet_routes() {
	
	t_rtentry * p_route;

	while(subnet_routes) {
		
		del_route(subnet_routes->dest, subnet_routes->gw, subnet_routes->metric, subnet_routes->mask, subnet_routes->table, ifce_index);
		p_route = subnet_routes;
		subnet_routes = (t_rtentry *) subnet_routes->next;
		p_free(p_route);
	}
}


