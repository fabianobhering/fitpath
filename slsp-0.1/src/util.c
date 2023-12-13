/**
 * @file
 * @author Diego Passos <dpassos@ic.uff.br>
 * @version 1.0
 *
 * @section DESCRIPTION
 *
 * Implementação do módulo de utilidades. Funções
 * de manipulação de memória e tratamento de erros.
 */


#include "util.h"

#include <string.h>
#include <errno.h>

#if 0
/****************************************************************
 *                      Variáveis Globais                       *
 ****************************************************************/

/**
 * Tabela de alocações de memória. Utilizado para debug apenas.
 */
static t_alloc_node * alloc_table = NULL;

#endif

/****************************************************************
 *                        Implementações                        *
 ****************************************************************/

/**
 * Função de impressão de mensagens de erro.
 * @param e Tipo do erro.
 * @param function String contendo o nome da função na qual o erro ocorreu.
 * @return Nada.
 */
void p_error(t_error_type e, const char * function){
	
	FILE * err;

	if (daemonize) {

		err = fopen("/tmp/protocolo.log", "w");
	}
	else
		err = stderr;

	switch(e) {
		case out_of_memory: 
			fprintf(err, "Erro na função %s: falta de memória.\n\n", function);
			break;
		case double_free: 
			fprintf(err, "Erro na função %s: tentativa de liberação de memória não alocada.\n\n", function);
			break;
		case socket_error:
			fprintf(err, "Erro na função %s: falha ao abrir socket.\n\n", function);
			break;
		case bind_error:
			fprintf(err, "Erro na função %s: falha de bind.\n\n", function);
			break;
		case sysctl_error:
			fprintf(err, "Erro na função %s: erro em chamada ao sistema.\n\n", function);
			break;
		case setsockopt_error:
			fprintf(err, "Erro na função %s: falha ao tentar configurar o socket.\n\n", function);
			break;
		case info_mismatch:
			fprintf(err, "Erro na função %s: nó existe na lista de pacotes, mas não no grafo.\n\n", function);
			break;
		case file_open:
			fprintf(err, "Erro na função %s: falha ao abrir arquivo.\n\n", function);
			break;
	}
	
        fprintf(err, "%p\n", __builtin_return_address(0));
        fprintf(err, "%p\n", __builtin_return_address(1));
        fprintf(err, "%p\n", __builtin_return_address(2));
        fprintf(err, "%p\n", __builtin_return_address(3));
        fprintf(err, "%p\n", __builtin_return_address(4));

	if (daemonize) fclose(err);

#if 0
	/* Status de execução. */
	
	status = e + 1;
	
	/* Parar as threads. */
	
	run = 0;

	/* Liberar o processo principal para o encerramento. */
	
	sem_post(&finalize_mutex);
	
	pthread_kill(pthread_self(), SIGINT);
#endif
}

/**
 * Função de impressão de mensagens de warning.
 * @param e Tipo do warning.
 * @param function String contendo o nome da função na qual o warning ocorreu.
 * @return Nada.
 */
void p_warning(t_warning_type e, const char * function){
	
	switch(e) {
		case alloc_not_empty: 
			fprintf(stderr, "Aviso na função %s: tabela de alocação ainda contém registros.\n\n", function);
			break;
		case unknown_pkt:
			fprintf(stderr, "Aviso na função %s: recebido pacote desconhecido. Ignorando.\n\n", function);
			break;
		case checksum_fail:
			fprintf(stderr, "Aviso na função %s: checksum inválido para pacote recebido. Ignorando.\n\n", function);
			break;
		case no_graph_node:
			fprintf(stderr, "Aviso na função %s: nó não existente no grafo. Ignorando.\n\n", function);
			break;
		case bipart_graph:
			fprintf(stderr, "Aviso na função %s: grafo ainda bipartido.\n\n", function);
			break;
		case bad_route:
			fprintf(stderr, "Aviso na função %s: erro na modificação da tabela de rotas.\n\n", function);
			break;
	}
}

#ifdef	DEBUG

/**
 * Imprime uma mensagem de debug, contendo o nome da função
 * que requisitou a impressão.
 * @param msg String com a mensagem a ser impressa.
 * @param function String com o nome da função.
 * @return Nada.
 */
void d_printf(char * msg, const char *function) {
	
	fprintf(stderr, "%s: %s", function, msg);	
}

#else

/**
 * Versão da função para debug desabilitado.
 */
void d_printf(char * msg, const char *function) {

}

#endif

#if 0

/**
 * Faz um dump da tabela de alocações do processo. Para propósitos de debug.
 * @return Nada.
 */
void dump_allocTable() {

	FILE * memlog;
	t_alloc_node * p;
	int totalSize = 0;

	p = alloc_table;

	if (daemonize) {

		memlog = fopen("protocolo.memlog", "w");
	}
	else
		memlog = stderr;

	fprintf(memlog, "Lista de alocacoes.\n");
	while (p) {

		fprintf(memlog, "%d bytes de memoria alocada na posição %p. Alocada por %s (%s:%hu)\n", p->size, p->addr, p->func, p->file, p->line);

		totalSize += p->size + sizeof(t_alloc_node);
		p = (t_alloc_node *) p->next;	
	}
	fprintf(memlog, "Total alocado: %d bytes.\n", totalSize);

	if (daemonize) fclose(memlog);
}

/**
 * Função de alocação de memória a ser usada no modo de debug. Alocações são guardadas 
 * em uma tabela.
 * @param size Tamanho da área de memória a ser alocada.
 * @param func Nome da função que requisitou a alocação.
 * @param line Número da linha na qual a alocação foi requisitada.
 * @return Ponteiro para a região de memória alocada.
 */
void *__p_alloc__(unsigned int size, const char * func, const char * file, unsigned short line) {

	char debug_msg[MAX_DEBUG_MSG_SIZE];	// Mensagem de debug.
	t_alloc_node *new_alloc;
	
	/* Criar um novo nó da tabela de alocação. */
	
try1:
	errno = 0;
	if ((new_alloc = (t_alloc_node *) malloc(sizeof(t_alloc_node))) == NULL || errno != 0) {

		if (errno != ENOMEM) goto try1;
		sprintf(debug_msg, "p_alloc (chamada a partir de %s:%d, tentando alocar %d bytes, com errno = %d)", file, line, sizeof(t_alloc_node), errno);
		dump_allocTable();
		p_error(out_of_memory, debug_msg);
	}
	
	/* Tentar alocar. */
	
	new_alloc->size = size;
	strcpy(new_alloc->func, func);
	strcpy(new_alloc->file, file);
	new_alloc->line = line;
try2:
	errno = 0;
	new_alloc->addr = malloc(size);
	
	/* Testar se a alocação teve êxito. */

	if (new_alloc->addr == NULL || errno != 0) {
		
		if (errno != ENOMEM) goto try2;
		sprintf(debug_msg, "p_alloc (chamada a partir de %s:%d, tentando alocar %d bytes, com errno = %d)", file, line, size, errno);
		dump_allocTable();
		p_error(out_of_memory, debug_msg);
	}
	
	/* Avisar ao usuário que estamos alocando memória. */

#ifdef	DEBUG1
	
	sprintf(debug_msg, "%d bytes de memória alocada na posição %d.\n", size, (unsigned) new_alloc->addr);
	d_printf(debug_msg, "p_alloc");

#endif
	
	/* Anotar na tabela de alocações. */
	
	new_alloc->next = (struct t_alloc_node *) alloc_table;
	alloc_table = new_alloc;
		
	return(new_alloc->addr);
}

/**
 * Função de desalocação de memória no modo de debug. Tira a alocação da lista
 * e verifica por free's em posições inválidas.
 * @param addr Endereço da região a ser desalocada.
 * @param file Nome do arquivo no qual a desalocação foi requisitada.
 * @param line Número da linha na qual a desalocação foi requisitada.
 * @return Nada.
 */
void __p_free(void *addr, char * file, int line){

	char debug_msg[MAX_DEBUG_MSG_SIZE];	// Mensagem de debug.
	t_alloc_node *p = alloc_table, *prev = NULL;

	while (p)
	{
		/* Encontramos o endereço na tabela? */
		
		if (p->addr == addr) {
			free(addr);
			
			/* Avisar ao usuário que estamos liberando memória. */
		
#ifdef	DEBUG1

			sprintf(debug_msg, "%d bytes de memória liberada na posição %d.\n", p->size, (unsigned) addr);
			d_printf(debug_msg, "p_free");

#endif
			
			/* Tirar 'p' da tabela. */
			
			if (prev) prev->next = p->next;
			else alloc_table = (t_alloc_node *) p->next;
			
			free(p);
		
			/* A busca terminou com sucesso. */
			
			return;
		}
		
		prev = p;
		p = (t_alloc_node *) p->next;	
	}
	
	dump_allocTable();

	/* Erro: tentativa de desalocar uma área não alocada. */
	
	sprintf(debug_msg, "p_free (chamada a partir de %s:%d)", file, line);
	p_error(double_free, debug_msg);

free(addr);
}

/**
 * Libera toda a memória alocada na tabela de alocações.
 * @return Nada.
 */
void free_alloc_table(void) {

#ifdef	DEBUG

	char debug_msg[MAX_DEBUG_MSG_SIZE];	// Mensagem de debug.

#endif
	
	t_alloc_node *p;
	
	/* Se liberarmos a memória da tabela de alocação, não será possível 
	 * utilizar a função 'p_free()' para desalocar memória. Por isso, devemos 
	 * desalocar toda a memória alocada indicada pela tabela antes de tirá-la.
	 * Esta função só deve ser usada no encerramento do programa, para guarantir
	 * que não causaremos 'licking' de memória. */

	if (alloc_table) {

		/* Avisar ao usuário que estamos encerrando a tabela com alocações ainda
		 * pendentes: algo errado está acontecendo. */

		p_warning(alloc_not_empty, "free_alloc_table");

		/* Desalocar cada entrada da tabela. */
		
		do {			

#ifdef	DEBUG

			sprintf(debug_msg, "%d bytes de memória liberada na posição %d. Alocada por %s (%s:%hu)\n", alloc_table->size, (unsigned) alloc_table->addr, alloc_table->func, alloc_table->file, alloc_table->line);
			d_printf(debug_msg, "p_free");

#endif
			
			free(alloc_table->addr);
			
			p = (t_alloc_node *) alloc_table->next;
			free(alloc_table);
			alloc_table = p;
		} while(alloc_table);
	}
}

#endif

/**
 * Função de alocação segura de memória para processos multi-thread. 
 * Verifica o valor de errno e desiste apenas se a mensagem é um ENOMEM.
 * @param size Tamanho da área requisitada para alocação.
 * @return Ponteiro para a área de memória recém alocada.
 * @note A função nunca retorna um ponteiro inválido. Em caso de erro,
 * todo o processo é abortado.
 */
void * p_alloc(unsigned int size) {

	void * addr;

	while(1) {

		errno = 0;
		addr = malloc(size);
		
		/* Testar se a alocação teve êxito. */

		if (addr == NULL || errno != 0) {
			
			if (errno != ENOMEM) continue;
			p_error(out_of_memory, "p_alloc");
		}

		break ;
	}	

	return(addr);
}

/**
 * Remove a extensão no nome de um arquivo.
 * @param s String contendo o nome do arquivo.
 * @return Ponteiro para a string resultante.
 * @note A área de memória retornada é sempre a mesma. Portanto, o usuário deve 
 * terminar o uso do resultado (ou copiar para outra área de memória) antes
 * de chamar esta função novamente.
 */
char * loseExt(char * s) {

	static char outputBuffer[100];
	int i;
	int len = strlen(s);

	for (i = 0; i < len; i++) {

		if (s[i] == '.') break;
		outputBuffer[i] = s[i];
	}

	return(outputBuffer);
}

/**
 * Extrai apenas o nome do arquivo em uma representação contendo todo o 
 * caminho (absoluto ou não).
 * @param s String contendo o caminho para o arquivo.
 * @return Ponteiro para a string resultante.
 * @note A área de memória retornada é sempre a mesma. Portanto, o usuário deve 
 * terminar o uso do resultado (ou copiar para outra área de memória) antes
 * de chamar esta função novamente.
 */
char * steam(char * s) {

	static char outputBuffer[100];
	int i;
	int j;
	int len = strlen(s);

	memset(outputBuffer, 0, 100);

	j = 0;
	for (i = 0; i < len; i++) {

		if (s[i] == '/') j = 0;
		else outputBuffer[j++] = s[i];
	}

	return(outputBuffer);
}

/**
 * Cria uma representação em string da hora atual.
 * @param buf Buffer para acomodação do resultado.
 * @return Ponteiro para buf.
 */
char * get_current_time(char * buf) {

        static char s_now[DATE_STRING_LEN];
        time_t now;

        now = time(NULL);

        if (buf) {

                ctime_r(& now, buf);
		buf[strlen(buf) - 1] = 0;
                return(buf);
        }
        else {

                ctime_r(& now, s_now);
		s_now[strlen(s_now) - 1] = 0;
                return(s_now);
        }
}

