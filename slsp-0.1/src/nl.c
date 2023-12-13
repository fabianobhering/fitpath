/**
 * @file
 * @author Diego Passos <dpassos@ic.uff.br>
 * @version 1.0
 *
 * @section DESCRIPTION
 *
 * Módulo que implementa rotinas de manipulação do netlink.
 */


#include <string.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "nl.h"

/**
 * Handler para manipulação de rotas.
 */
static nl_handle rth;

/**
 * Função que envia requisições para o netlink.
 * @param n Ponteiro para a estrutura da mensagem a ser enviada.
 * @return 0, em caso de sucesso. 
 */
int rtnl_send_request(struct nlmsghdr *n) {

	int status;
	struct sockaddr_nl nladdr;
	struct nlmsgerr * err;
	struct iovec iov = {
		.iov_base = (void*) n,
		.iov_len = n->nlmsg_len
	};
	struct msghdr msg = {
		.msg_name = &nladdr,
		.msg_namelen = sizeof(nladdr),
		.msg_iov = &iov,
		.msg_iovlen = 1,
	};
	char   buf[256];

	/*
	 * Endereco do destino (kernel).
	 */

	memset(&nladdr, 0, sizeof(nladdr));
	nladdr.nl_family = AF_NETLINK;

	/*
	 * Parametros da mensagem (numero de sequencia e flags).
	 */

	n->nlmsg_seq = ++rth.seq;
	n->nlmsg_flags |= NLM_F_ACK;

	/*
	 * Envio da mensagem.
	 */

	status = sendmsg(rth.fd, &msg, 0);

	/*
	 * Status do envio.
	 */

	if (status < 0) {

		perror("(Cannot talk to rtnetlink)");
		return(-1);
	}

	/*
	 * Preparacao do buffer de recepcao da resposta.
	 */

	memset(buf,0,sizeof(buf));

	iov.iov_base = buf;
	iov.iov_len = sizeof(buf);

	/*
	 * Recepcao.
	 */

	status = recvmsg(rth.fd, &msg, 0);

	/*
	 * Status da recepcao.
	 */

	if (status < 0) {
		perror("(Cannot talk to rtnetlink)");
		return(-1);
	}

	/*
	 * A mensagem eh sempre de erro (presencao ou ausencia).
	 */

	err = (struct nlmsgerr *) NLMSG_DATA((struct nlmsghdr *) buf);

	if (err->error) {

		errno = -err->error;
		perror("RTNETLINK answers");
	}

	return(-err->error);
}

/**
 * Função que inicializa o módulo.
 * @return Nada.
 */
void rtnl_init() {

	/*
	 * Zerar a estrutura que abriga as parametros 
	 * da conexao NETLINK.
	 */

	memset(& rth, 0, sizeof(rth));

	/*
	 * Tentar abrir um socket NETLINK.
	 */

	rth.fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (rth.fd < 0) {
		perror(" (socket())");
	}

	/*
	 * Preencher a estrutura com o nosso endereco.
	 */

	memset(& (rth.local), 0, sizeof(rth.local));
	rth.local.nl_family = AF_NETLINK;
	rth.local.nl_groups = 0;

	/*
	 * Tentar fazer o bind.
	 */

	if (bind(rth.fd, (struct sockaddr *) (& (rth.local)), sizeof(rth.local)) < 0) {
		perror("(bind())");
	}

	/*
	 * Iniciar o numero de sequencia.
	 */

	rth.seq = 0;
}

/*
 * As proximas duas funcoes e o proximo macro adicionam um atributo a uma requisicao de rotas
 * atraves de uma mensagem netlink. Ambas foram adaptadas a partir do codigo 
 * do arquivo libnetlink.c do pacote iproute2.
 */

/**
 * Retorna o final de uma mensagem netlink.
 * @param nmsg Ponteiro para uma mensagem netlink.
 * @return Ponteiro para o final da mensagem netlink.
 */
#define NLMSG_TAIL(nmsg) ((struct rtattr *) (((void *) (nmsg)) + NLMSG_ALIGN((nmsg)->nlmsg_len)))

/**
 * Adiciona um atributo de 4 bytes a uma mensagem netlink.
 * @param n Ponteiro para a mensagem.
 * @param maxlen Não é usado.
 * @param type Tipo do atributo.
 * @param data Dados do atributo.
 * @return Nada.
 */
void addattr32(struct nlmsghdr *n, int maxlen, int type, __u32 data) {

	int len = RTA_LENGTH(4);
	struct rtattr *rta;

	rta = NLMSG_TAIL(n);
	rta->rta_type = type;
	rta->rta_len = len;
	memcpy(RTA_DATA(rta), &data, 4);
	n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + len;
}

/**
 * Adiciona um atributo longo a uma mensagem netlink.
 * @param n Ponteiro para a mensagem.
 * @param maxlen Não utilizado.
 * @param type Tipo do atributo.
 * @param data Ponteiro para os dados.
 * @param alen Tamanho dos dados (em bytes).
 * @return Nada.
 */
void addattr_l(struct nlmsghdr *n, int maxlen, int type, const void *data, int alen) {

	int len = RTA_LENGTH(alen);
	struct rtattr *rta;

	rta = NLMSG_TAIL(n);
	rta->rta_type = type;
	rta->rta_len = len;
	memcpy(RTA_DATA(rta), data, alen);
	n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len);
}

/**
 * Finaliza o módulo.
 * @return Nada.
 */
void rtnl_fini() {

	close(rth.fd);
}

