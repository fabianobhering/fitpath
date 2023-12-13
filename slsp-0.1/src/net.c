/**
 * @file
 * @author Diego Passos <dpassos@ic.uff.br>
 * @version 1.0
 *
 * @section DESCRIPTION
 *
 * Módulo que implementa funções utilitárias para manipulação e configuração
 * de serviços de rede.
 */


#include "net.h"

#include <linux/sockios.h>
#include <errno.h>

/****************************************************************
 *                      Variáveis Globais                       *
 ****************************************************************/

/**
 * Socket para envio dos pacotes de controle.
 */
static int send_sock;

/**
 * Socket para a recepção de pacotes de controle.
 */
static int recv_sock;


/****************************************************************
 *                        Implementações                        *
 ****************************************************************/

/**
 * Retorna o índice (interno do kernel) da interface especificada.
 * @param ifce Nome da interface.
 * @return Índice retornado pela chamada de sistema.
 */
int get_ifce_index(char * ifce) {

        struct ifreq if_data;
        int sockd;          

        /* Abrir um socket para a chamada de sistema. */

        if ((sockd = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
		p_error(socket_error, "get_ifce_index");

        /* Copiar o nome da interface para a estrutura. */

        strcpy(if_data.ifr_name, ifce);

        /* Configurar o tipo de endereço. */

        if_data.ifr_addr.sa_family = AF_INET;

        /* Fazer a chamada de sistema correspondente. */

        if (ioctl(sockd, SIOCGIFINDEX, &if_data) < 0)
		p_error(sysctl_error, "get_ifce_index");

	return(if_data.ifr_ifru.ifru_ivalue);
}

/**
 * Retorna o endereço IP da interface especificada.
 * @param ifce Nome da interface.
 * @return Endereço IP.
 */
t_ip_addr get_ifce_ipaddr(char * ifce) {
	
	struct ifreq if_data;		// Estrutura que guarda as informações da ifce. 
	int sockd;					// Socket para a chamada de sistema.
	t_ip_addr ip;
	
	/* Abrir um socket para a chamada de sistema. */
	
	if ((sockd = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
		p_error(socket_error, "get_ifce_ipaddr");

	/* Copiar o nome da interface para a estrutura. */

	strcpy(if_data.ifr_name, ifce);
	
	/* Configurar o tipo de endereço. */
	
	if_data.ifr_addr.sa_family = AF_INET;

	/* Fazer a chamada de sistema correspondente. */
	
	if (ioctl(sockd, SIOCGIFADDR, &if_data) < 0)
		p_error(sysctl_error, "get_ifce_ipaddr");

	/* Copiar o resultado. */
	
	memcpy((void *) &ip, (void *) &(((struct sockaddr_in *) &if_data.ifr_addr)->sin_addr), 4);
	
	/* Fechar o socket. */
	
	close(sockd);
	
	/* Retornar o ip na forma de string. */
	
	return(ip);

}

/**
 * Retorna o endereço de broadcast da sub-rede da interface especificada.
 * @param ifce Nome da interface.
 * @return Endereço IP de broadcast.
 */
t_ip_addr get_ifce_bcastaddr(char * ifce) {
	
	struct ifreq if_data;		// Estrutura que guarda as informações da ifce. 
	int sockd;					// Socket para a chamada de sistema.
	t_ip_addr ip;
	
	/* Abrir um socket para a chamada de sistema. */
	
	if ((sockd = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
		p_error(socket_error, "get_ifce_bcastaddr");

	/* Copiar o nome da interface para a estrutura. */

	strcpy(if_data.ifr_name, ifce);

	/* Fazer a chamada de sistema correspondente. */
	
	if (ioctl(sockd, SIOCGIFBRDADDR, &if_data) < 0)
		p_error(sysctl_error, "get_ifce_bcastaddr");

	/* Copiar o resultado. */
	
	memcpy((void *) &ip, (void *) &if_data.ifr_broadaddr.sa_data + 2, 4);

	/* Fechar o socket. */
	
	close(sockd);
	
	/* Retornar o ip na forma de string. */
	
	return(ip);
	
}

/**
 * Converte um endereço IP em uma string.
 * @param ip Endereço IP.
 * @return String representando o IP.
 */
char * inetaddr(t_ip_addr ip) {

	struct in_addr in;
		
	in.s_addr = ip;
	return inet_ntoa(in);
}

/**
 * Cria um socket para envio.
 * return Nada.
 */
void create_send_sock() {

	int optval = 1;						// Opção do socket.

	/* Criar o socket. */
	
	if ((send_sock = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
		p_error(socket_error, "create_send_sock");
	
	/* Setar a opção de aceitar broadcast. */
	
	if ((setsockopt(send_sock, SOL_SOCKET, SO_BROADCAST, (caddr_t) &optval, sizeof(optval))))
		p_error(setsockopt_error, "create_send_sock");		
}

/**
 * Cria um socket para recepção.
 * @return Nada.
 */
void create_recv_sock() {

	int optval = 1;
	struct sockaddr_in localaddr;

	/* Setar o endereço no qual a interface responderá. */
	
	localaddr.sin_family = AF_INET;
	localaddr.sin_port = htons(port);
	localaddr.sin_addr.s_addr = ifce_bcastaddr;
	
	/* Criar o socket. */
	
	if ((recv_sock = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
		p_error(socket_error, "create_recv_sock");
	
	/* Setar a opção de aceitar broadcast. */
	
	if ((setsockopt(recv_sock, SOL_SOCKET, SO_BROADCAST, (caddr_t) &optval, sizeof(optval))))
		p_error(setsockopt_error, "create_recv_sock");		

	/* Fazer o "bind" no endereço de broadcast. */
	
	if ((bind(recv_sock, (struct sockaddr *) &localaddr, sizeof(struct sockaddr))) == -1)
		p_error(bind_error, "create_recv_sock");
	
}

/**
 * Envia um pacote pela interface de controle.
 * @param pkt Ponteiro para o buffer do pacote.
 * @param pkt_size Tamanho (em bytes) do pacote.
 * @param target Endereço IP de destino.
 * @return Nada.
 */
void p_send(void * pkt, size_t pkt_size, t_ip_addr target) {

	struct sockaddr_in dest;

	/* Configurar o destino. */
	
	dest.sin_family = AF_INET;
	dest.sin_port = htons(port);
	dest.sin_addr.s_addr = target;

	/* Esperar pelo mutex. */
	
	sem_wait(&send_sock_mutex);
	
	/* Enviar o pacote. */
	
try_send:

	errno = 0;
	sendto(send_sock, pkt, pkt_size, 0, (struct sockaddr *) &dest, sizeof(struct sockaddr));
	if (errno == EINTR) goto try_send;

	/* Liberar o mutex. */
	
	sem_post(&send_sock_mutex);
}

/**
 * Recebe um pacote da interface de controle.
 * @param pkt Buffer onde o pacote recebido será colocado.
 * @param sender Ponteiro para área de memória onde o endereço da 
 * origem do pacote será guardada.
 * @return Tamanho do pacote.
 */
size_t p_recv(void * pkt, t_ip_addr * sender) {

	struct sockaddr_in senderaddr;
	socklen_t size = sizeof(struct sockaddr_in);
	size_t received;

	/* Receber o pacote. */
	
try_recv:
	
	errno = 0;
	received = recvfrom(recv_sock, pkt, pkt_max_size, 0, (struct sockaddr *) &senderaddr, &size);
	if (errno == EINTR) goto try_recv;

	/* Retirar a origem do pacote. */
	
	* sender = senderaddr.sin_addr.s_addr; 
	
	return(received);
}

/**
 * Fecha o socket de envio.
 * @return Nada.
 */
void destroy_send_sock(void) {
	
	close(send_sock);
}

/**
 * Fecha o socket de recepção.
 * @return Nada.
 */
void destroy_recv_sock(void) {
	
	close(recv_sock);
}

/* * * * * * * * * * * * * * * * * * * * * * * * *
 * Funcoes de manipulacao da taxa de transmissao.*
 * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * TODO: mover estas estruturas de dados para um local proprio do 
 * MARA. As funcoes neste arquivo deveriam ser o mais genericas 
 * possivel.
 */

/**
 * Vetor para fazer a conversão entre as quatro taxas de envio de probes
 * para os índices das taxas da camada de enlace.
 */
int mcastRate2Rate[] = {

	0, 
	7, 
	9, 
	11
};

/**
 * Número de classes de tamanho de pacotes.
 */
int nRateClasses = 4;

/**
 * Vetor de classes de tamanho de pacotes.
 */
int rateClasses[] = {

	350, 
	750, 
	1300, 
	1514
};

/**
 * Descritor de arquivos para o arquivo de manipulação das taxas
 * (pprs).
 */
FILE * pprs_proc_file = NULL;

/**
 * Vetor de taxas (nominais) disponíveis.
 */
double availRates[] = {

	1.0,
	2.0,
	5.5,
	6.0,
	9.0,
	11.0,
	12.0,
	18.0,
	24.0,
	36.0,
	48.0,
	54.0
};

/**
 * Carrega o módulo pprs.
 * @return 0 em caso de sucesso. -1, caso contrário.
 */
int net_load_pprs(void) {

#ifndef PC
	char cmd[101];

	sprintf(cmd, "%s %s queue_len=1000", insmod_path, pprs_path);
	system(cmd);

	pprs_proc_file = fopen(pprs_rates_file, "w");
	if (!pprs_proc_file) {

		perror("net_load_pprs");
		fprintf(stderr, "unable to open pprs rates file: %s\n", pprs_rates_file);
		return(-1);
	}
#endif
	return(0);
}

/**
 * Remove o módulo pprs.
 * @return 0 em caso de sucesso. -1, caso contrário.
 */
inline int net_unload_pprs(void) {

#ifndef PC
	char cmd[56];

	fclose(pprs_proc_file);	
	sprintf(cmd, "%s %s", rmmod_path, loseExt(steam(pprs_path)));
	return(system(cmd));
#else
	return(0);
#endif
}

/**
 * Cadastra as classes de tamanho de pacotes no módulo PPRS.
 * @return 0 em caso de sucesso. -1, caso contrário.
 */
int net_define_classes(void) {

#ifndef PC
	FILE * pprs_proc_file;
	char request[50];
	int i;

	pprs_proc_file = fopen(pprs_classes_file, "w");
	if (!pprs_proc_file) {

		perror("define_classes");
		fprintf(stderr, "unable to open file \"%s\"\n", pprs_classes_file);
		return(-1);
	}

	sprintf(request, "%d", rateClasses[0]);
	for (i = 1; i < nRateClasses; i++) {

		sprintf(request, "%s,%d", request, rateClasses[i]);
	}

	fprintf(pprs_proc_file, "%s\n", request);

	fclose(pprs_proc_file);	
#endif
	return(0);
}

/**
 * Computa a taxa nominal referente a uma taxa de transmissão indicada pelo índice.
 * @return Valor nominal da taxa (em Mb/s).
 */
inline double net_get_rate_from_index(int rateIndex) {

	if (rateIndex >= sizeof(availRates) || rateIndex < 0) {

		return(-1.0);
	}

	return(availRates[rateIndex]);
}

/**
 * Altera a taxa de transmissão da interface para um vizinho.
 * @param newrate Índice da taxa de transmissão.
 * @param ip Endereço IP do vizinho.
 * @return Nada.
 */
void net_set_ifce_rate(char newrate, const t_ip_addr ip) {

#ifndef PC
	int i;
	float rate;

	if (newrate != -1)
		rate = net_get_rate_from_index(newrate);
	else
		rate = -1.0;	  

	for (i = 0; i < nRateClasses; i++) {

		fprintf(pprs_proc_file, "U %s %d %.1f\n", inetaddr(ip), i, rate);
		fflush(pprs_proc_file);	
	}
#endif
}

/**
 * Altera a taxa de transmissão da interface para um vizinho para uma classe de tamanho
 * específica.
 * @param newrate Índice da taxa de transmissão.
 * @param ip Endereço IP do vizinho.
 * @param size_class Índice da classe de tamanho.
 * @return Nada.
 */
void net_set_ifce_rate_for_size(char newrate, const t_ip_addr ip, int size_class) {

#ifndef PC
	float rate;

	if (size_class < 0 || size_class >= nRateClasses) 
		return ;

	if (newrate != -1)
		rate = net_get_rate_from_index(newrate);
	else
		rate = -1.0;	  

	fprintf(pprs_proc_file, "U %s %d %.1f\n", inetaddr(ip), size_class, rate);
	fflush(pprs_proc_file);	
#endif
}

