/**
 * @file
 * @author Diego Passos <dpassos@ic.uff.br>
 * @version 1.0
 *
 * @section DESCRIPTION
 *
 * Programa principal. Cuida da inicialização e término.
 *
 * @mainpage
 *
 * @author Diego Passos <dpassos@ic.uff.br>
 *
 * O SLSP é um protocolo de roteamento simples, baseado em estado de enlaces, para 
 * redes em malha sem fio. Este protocolo implementa 4 métricas diferentes:
 * - Hop Count.
 * - ETX.
 * - ML.
 * - MARA (e variações).
 *
 * A métrica, assim como outros parâmetros de configuração, pode ser especificada
 * através do arquivo de configuração passado como parâmetro para o executável.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define __USE_GNU
#include <pthread.h>
#undef __USE_GNU

#include <sched.h>
#include <signal.h>

#include "packets.h"
#include "routes.h"
#include "cfg_parser.h"
#include "mara.h"
#include "topologyView.h"
#include "defs.h"
#include "nl.h"
#include "signal_handler.h"

/**
 * Caminho para o arquivo do /proc que permite habilitar forward.
 */
#define FORWARDING_PROC_FILE	"/proc/sys/net/ipv4/ip_forward"

/**
 * Caminho para o arquivo do /proc que permite desabilitar o envio 
 * de redirects.
 */
#define REDIRECTS_PROC_FILE	"/proc/sys/net/ipv4/conf/%s/send_redirects"

/**
 * Thread de recebimento de pacotes.
 */
pthread_t * recvThread;

/**
 * Thread de envio de pacotes.
 */
pthread_t * sendThread;

/**
 * Thread de visualização da topologia.
 */
pthread_t * tvThread;

/*
 * TODO: Normalizar implementacao.
 */

/**
 * Habilita forward de pacotes no nó.
 * @return Nada.
 */
void enableForwarding() {

	FILE * pf;

	pf = fopen(FORWARDING_PROC_FILE, "w");
	if (pf == NULL) {

		fprintf(stderr, "Falha ao tentar habilitar o forwarding...\n");
		exit(1);
	}

	fprintf(pf, "1\n");

	fclose(pf);
}

/**
 * Desabilita forward de pacotes no nó.
 * @return Nada.
 */
void disableForwarding() {

	FILE * pf;

	pf = fopen(FORWARDING_PROC_FILE, "w");
	if (pf == NULL) {

		fprintf(stderr, "Falha ao tentar desabilitar o forwarding...\n");
		exit(1);
	}

	fprintf(pf, "0\n");

	fclose(pf);
}

/**
 * Habilita o envio de redirects no nó.
 * @return Nada.
 */
void enableRedirects() {

	FILE * pf;
	char resName[200];

	sprintf(resName, REDIRECTS_PROC_FILE, ifce_name);
	pf = fopen(resName, "w");
	if (pf == NULL) {

		fprintf(stderr, "Falha ao tentar habilitar o ICMP redirect...\n");
		exit(1);
	}

	fprintf(pf, "1\n");

	fclose(pf);

	sprintf(resName, REDIRECTS_PROC_FILE, "all");
	pf = fopen(resName, "w");
	if (pf == NULL) {

		fprintf(stderr, "Falha ao tentar habilitar o ICMP redirect...\n");
		exit(1);
	}

	fprintf(pf, "1\n");

	fclose(pf);
}

/**
 * Desabilita o envio de redirects no nó.
 * @return Nada.
 */
void disableRedirects() {

	FILE * pf;
	char resName[200];

	sprintf(resName, REDIRECTS_PROC_FILE, ifce_name);
	pf = fopen(resName, "w");
	if (pf == NULL) {

		fprintf(stderr, "Falha ao tentar desabilitar o ICMP redirect...\n");
		exit(1);
	}

	fprintf(pf, "0\n");

	fclose(pf);

	sprintf(resName, REDIRECTS_PROC_FILE, "all");
	pf = fopen(resName, "w");
	if (pf == NULL) {

		fprintf(stderr, "Falha ao tentar desabilitar o ICMP redirect...\n");
		exit(1);
	}

	fprintf(pf, "0\n");

	fclose(pf);

}

/**
 * Inicializa estruturas do protocolo.
 * @return Nada.
 */
void p_init() {

	/* Pegar a data de inicio: pode ser util para debug. */

	get_current_time(start_date);

	/* Carregar o modulo PPRS (se necessario). */

	if (lq_level > 2) { /* Maior que ML. */

		if (net_load_pprs()) exit(1);
		if (net_define_classes()) exit(1);
		if (mara_init(per_table_path)) exit(1);
		if (lq_level == 5) mararp_init();
	}

	/* Habilitar forwarding e desabilitar ICMP redirect. */

	enableForwarding();
	disableRedirects();

	/* Iniciar parâmetros do programa. */
	
	ifce_ipaddr = get_ifce_ipaddr(ifce_name);
	ifce_bcastaddr = get_ifce_bcastaddr(ifce_name);
	
	/* Criar os sockets. */
	
	create_send_sock();
	create_recv_sock();

	/* Iniciar o mutex do socket de envio. */
	
	sem_init(&send_sock_mutex, 0, 1);
	
	/* Iniciar o mutex de finalização. */
	
	sem_init(&finalize_mutex, 0, 0);
	
	/* Iniciar o mutex da vizinhança. */
	
	sem_init(&neigh_mutex, 0, 1);
	
	/* Iniciar o mutex da topologia. */
	
	sem_init(&topo_mutex, 0, 1);
	
	/* Iniciar o manipulador de rotas e regras de roteamento. */

	rtnl_init();
/*
 * Codigo antigo. Não remover, por enquanto.
 */
#if 0 	
	init_routes();
#endif

}

/**
 * Executa rotinas de encerramento do protocolo.
 * @return Nada.
 */
void p_finalize() {
	
	/* Remover o modulo do PPRS (se necessario). */

	if (lq_level > 2) { /* Maior que ML. */

		net_unload_pprs();
		mara_finalize();
		if (lq_level == 5) mararp_finalize();
	}

	/* Desabilitar o forwarding e o ICMP redirect. */

	disableForwarding();
	enableRedirects();

	/* Finalizar o gerenciador de rotas. */

	finalize_routes();
	rtnl_fini();

	/* Liberar toda a memória alocada pelos módulos. */
	
	free_neigh();
	
	free_graph();
	
	free_tc_list();
	
	free_defs();
/*
 * Codigo antigo. Não remover, por enquanto.
 */
#if 0	
	free_alloc_table();
#endif
		
	/* Fechar os sockets de comunicação. */
	
	destroy_send_sock();

	destroy_recv_sock();
	
	sem_destroy(&send_sock_mutex);
}

/**
 * Loop principal da thread de envio de pacotes.
 * @param parm Não utilizado.
 * @return Nada.
 */
void * sender_proc(void * parm) {
	
	t_packet 	* hello_pkt,
			* topology_pkt;
	size_t		  hello_pkt_size,
			  topology_pkt_size;
	unsigned char	* serial_hello_pkt,
			* serial_topology_pkt,
			  currHelloRate;
	unsigned int	  hello_remaining_time, 
			  topology_remaining_time,
			  next_intervall;
	

	/* Acertar os tempos restantes para os próximos envios. */
	
	hello_remaining_time = hello_intervall;
	topology_remaining_time = topology_intervall;

	/* Ajustar a taxa de transmissao inicial dos pacotes de hello. */

	currHelloRate = 0;

	/* A thread continuará até que o processo 
	 * principal altere o valor de 'run'. */
	
	while(run) {
		
		/* Verificar em quanto tempo é o próximo envio. */
		
		next_intervall = (hello_remaining_time < topology_remaining_time)?hello_remaining_time:topology_remaining_time;
		
		/* Aguardar o intervalo necessário. */
		
		sleep(next_intervall);
		
		/* Descobrir quais pacotes devem ser enviados agora. */
		
		if (!(hello_remaining_time -= next_intervall)) {
			
			/* Gerar um pacote de hello. */
	
			hello_pkt = gen_hello_pkt(currHelloRate);
	
			/* Serializá-lo. */
		
			serial_hello_pkt = serialize_pkt(hello_pkt, &hello_pkt_size);

			free_pkt(hello_pkt);

			/* Enviar. */
	
			p_send((void *) serial_hello_pkt, hello_pkt_size, ifce_bcastaddr);

			/* Avancar o currHelloRate (se necessario). */

			if (lq_level > 2)
				currHelloRate = (currHelloRate + 1) % 4;

			/* Liberar a memoria. */

			p_free(serial_hello_pkt);
			
			/* Vamos recomeçar o temporizador de pacotes de hello. */
			
			hello_remaining_time = hello_intervall;
		}
		
		if (!(topology_remaining_time -= next_intervall)) {
			
			/* Os pacotes de topologia são gerados dinamicamente. */
			
			topology_pkt = gen_topology_pkt();
			serial_topology_pkt = serialize_pkt(topology_pkt, &topology_pkt_size);

			free_pkt(topology_pkt);
			
			/* Enviar o pacote de topology. */
			
			p_send((void *) serial_topology_pkt, topology_pkt_size, ifce_bcastaddr);
			
			/* Liberar a memória do pacote serial. */
			
			p_free(serial_topology_pkt);
			
			/* Vamos recomeçar o temporizador de pacotes de hello. */
			
			topology_remaining_time = topology_intervall;
		}
			
	}
	
	/* Fim da thread. */
	
	return(NULL);
}

/**
 * Loop principal da thread de recebimento de pacotes.
 * @param parm Não utilizado.
 * @return Nada.
 */
void * receiver_proc(void * parm) {

	unsigned char 	* serial_pkt;
	t_packet	* pkt;
	size_t		  serial_pkt_size;
	t_ip_addr	  sender;
	
	/* Alocar memória para o buffer, onde os pacotes serão recebidos. */
	
	serial_pkt = (unsigned char *) p_alloc(pkt_max_size);
	
	/* A thread continuará até que o processo 
	 * principal altere o valor de 'run'. */
	
	while(run) {
		
		/* Aguardar um pacote. */
		
		serial_pkt_size = p_recv(serial_pkt, & sender);
		
		/* Novo pacote. Deserializar. */
		
		pkt = deserialize_pkt(serial_pkt, serial_pkt_size);
		
		/* Fazer parsing do pacote. */
		
		parse_pkt(pkt, sender);
		
		/* Liberar a memória do pacote. */
		
		free_pkt(pkt);
		
		/* Fim do ciclo. */
	}
	
	/* Final da thread. Desalocar memória do buffer de pacotes. */
	
	p_free(serial_pkt);
	
	return(NULL);
}

/**
 * Função principal.
 */
int main(int argc, char ** argv) {
	
	/* 
	 * Antes de qualquer coisa, configurar os signals. 
	 */

	install_handler();

	/*
	 * Antes do fork, verificar a configuracao (se for passada).
	 */

	if (argc > 2) {

		fprintf(stderr, "Atencao: ignorando argumentos alem do segundo.\n");
		parse_cfg(argv[1]);
	}
	else if (argc == 2) parse_cfg(argv[1]);
	else parse_cfg(NULL);

	if (daemonize)
		daemon(1,0);

	/* Iniciar estruturas. */
		
	p_init();
	
	/* Criar as threads. */
	
	sendThread = p_alloc(sizeof(pthread_t));
	recvThread = p_alloc(sizeof(pthread_t));

	/*
	 * Exibicao da topologia ?.
	 */

	if (topologyView) {

		tvThread = p_alloc(sizeof(pthread_t));
		pthread_create(tvThread, NULL, topologyView_proc, (void *) NULL);	
	}
	
	pthread_create(sendThread, NULL, receiver_proc, (void *) NULL);	
	pthread_create(recvThread, NULL, sender_proc, (void *) NULL);	
	
	/* O processo principal apenas espera a finalização. */
	
	sem_wait(&finalize_mutex);
	
	/* Encerramento. Juntar as threads. */
	
	pthread_join(* sendThread, NULL);
	pthread_join(* recvThread, NULL);
	
	/* Liberar a memória alocada. */
	
	p_free(sendThread);
	p_free(recvThread);

	/*
	 * Exibicao da topologia ?.
	 */

	if (topologyView) {

		pthread_join(* tvThread, NULL);
		p_free(tvThread);
	}

	/* Finalizar. */
	
	p_finalize();
	
	return(0);
}
