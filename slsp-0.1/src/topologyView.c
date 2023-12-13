/**
 * @file
 * @author Diego Passos <dpassos@ic.uff.br>
 * @version 1.0
 *
 * @section DESCRIPTION
 *
 * Módulo que implementa um pequeno servidor responsável por enviar
 * informações sobre o estado da topologia conhecida.
 */


#include "topologyView.h"
#include "topology.h"
#include "defs.h"

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

/**
 * Envia dados através de um socket TCP sobre a topologia.
 * @param fd Descritor do socket previamente aberto.
 * @return Nada.
 */
void enviaDados(int fd) {

	t_graph_node * p;
	int i;
	char buffer[100];

	write(fd, "digraph G {\n", 12);

	sem_wait(&topo_mutex);
	p = graph;
	
        while(p) {

                for (i = 0; i < p->n_links; i++) {

			if (p->links[i].cost[0] != INFINITY) {

	                        sprintf(buffer, "\"%s\" -> ", inetaddr(p->ip));
				write(fd, buffer, strlen(buffer));
	                        sprintf(buffer, "\"%s\" [label=\"%.1f\"];\n", inetaddr(p->links[i].ip), p->links[i].cost[0]);
				write(fd, buffer, strlen(buffer));
			}
			if (lq_level == 5) {
					
				if (p->links[i].cost[1] != INFINITY) {

					sprintf(buffer, "\"%s\" -> ", inetaddr(p->ip));
					write(fd, buffer, strlen(buffer));
					sprintf(buffer, "\"%s\" [label=\"%.1f\"];\n", inetaddr(p->links[i].ip), p->links[i].cost[1]);
					write(fd, buffer, strlen(buffer));
				}
				if (p->links[i].cost[2] != INFINITY) {

					sprintf(buffer, "\"%s\" -> ", inetaddr(p->ip));
					write(fd, buffer, strlen(buffer));
					sprintf(buffer, "\"%s\" [label=\"%.1f\"];\n", inetaddr(p->links[i].ip), p->links[i].cost[2]);
					write(fd, buffer, strlen(buffer));
				}
				if (p->links[i].cost[3] != INFINITY) {

					sprintf(buffer, "\"%s\" -> ", inetaddr(p->ip));
					write(fd, buffer, strlen(buffer));
					sprintf(buffer, "\"%s\" [label=\"%.1f\"];\n", inetaddr(p->links[i].ip), p->links[i].cost[3]);
					write(fd, buffer, strlen(buffer));
				}
			}
                }

                p = (t_graph_node *) p->next;
        }

	sem_post(&topo_mutex);

	write(fd, "}\n", 2);

	close(fd);
}

/**
 * Loop principal do servidor. Abre um socket TCP para escuta e aguarda
 * conexões.
 * @param param Não utilizado.
 * @return Nada (não utilizado).
 */
void * topologyView_proc(void * param) {

	int fd, clientSock;
	int flags;
	struct sockaddr_in sin;
	socklen_t sinLen;

	/*
	 * TODO: criar um handler decente para erros.
	 */

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {

		fprintf(stderr, "Erro ao abrir o socket:\n");
		perror(NULL);
		exit(1);
	}

	memset(& sin, 0, sizeof(sin));
	sin.sin_port = htons(2004);

	if (bind(fd, (struct sockaddr *) & sin, sizeof(sin))) {

		fprintf(stderr, "Erro ao fazer o bind na porta 2004:\n");
		perror(NULL);
		exit(1);
	}

	/*
	 * TODO:: cinco eh arbitrario.
	 */

	listen(fd, 5);

	/*
	 * Operacao nao bloqueante.
	 */

	flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
															                                
	while(run) {
	
		sinLen = sizeof(sin);
		clientSock = accept(fd, (struct sockaddr *) & sin, & sinLen);
		if (clientSock > 0) {

			enviaDados(clientSock);
		}

		sleep(1);
	}

	close(fd);

	return(NULL);
}

