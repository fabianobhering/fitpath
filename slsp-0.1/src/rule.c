/**
 * @file
 * @author Diego Passos <dpassos@ic.uff.br>
 * @version 1.0
 *
 * @section DESCRIPTION
 *
 * Módulo que implementa rotinas de manipulação da tabela de regras.
 */



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>

#include "rule.h"
#include "nl.h"

/*
 * Funcao de adicao de rotas.
 */

/**
 * Adiciona uma regra de roteamento.
 * @param fwmark Marca associada à regra.
 * @param table Tabela de roteamento de destino.
 * @return Nada.
 */
void rule_add(unsigned int fwmark, unsigned int table) {

	t_route_req req;

	/*
	 * Zerar toda a estrutura.
	 */

	memset(&req, 0, sizeof(req));

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL;
	req.n.nlmsg_type = RTM_NEWRULE;

	req.r.rtm_family = AF_INET;
	req.r.rtm_protocol = RTPROT_BOOT;
	req.r.rtm_scope = RT_SCOPE_UNIVERSE;
	req.r.rtm_type = RTN_UNICAST;

	/*
	 * Tabela alvo da regra.
	 */

	req.r.rtm_table = table;

	/*
	 * Adicionar a opcao RTA_PROTOINFO (marcacao do pacote).
	 */

	addattr32(& (req.n), sizeof(req), RTA_PROTOINFO, fwmark);

	/*
	 * Fazer o pedido ao kernel.
	 */

	rtnl_send_request(& (req.n));
}

/*
 * Funcao de remocao de rotas.
 */

/**
 * Remove uma regra de roteamento.
 * @param fwmark Marca assiciada à regra.
 * @param table Tabela de roteamento de destino da regra.
 * @return Nada.
 */
void rule_del(unsigned int fwmark, unsigned int table) {

	t_route_req req;

	/*
	 * Zerar toda a estrutura.
	 */

	memset(&req, 0, sizeof(req));

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.n.nlmsg_type = RTM_DELRULE;

	req.r.rtm_family = AF_INET;
	req.r.rtm_protocol = RTPROT_BOOT;
	req.r.rtm_scope = RT_SCOPE_UNIVERSE;
	req.r.rtm_type = RTN_UNSPEC;

	/*
	 * Tabela alvo da regra.
	 */

	req.r.rtm_table = table;

	/*
	 * Adicionar a opcao RTA_PROTOINFO (marcacao do pacote).
	 */

	addattr32(& (req.n), sizeof(req), RTA_PROTOINFO, fwmark);

	/*
	 * Fazer o pedido ao kernel.
	 */

	rtnl_send_request(& (req.n));
}

