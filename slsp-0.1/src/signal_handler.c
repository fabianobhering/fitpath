/**
 * @file
 * @author Diego Passos <dpassos@ic.uff.br>
 * @version 1.0
 *
 * @section DESCRIPTION
 *
 * Módulo que implementa rotinas de tratamento de sinais
 * recebidos pelo programa.
 */



#define _GNU_SOURCE
#include <dlfcn.h>

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>

#include "signal_handler.h"
#include "defs.h"
#include "util.h"

/*
 * Implementacao da manipulacao de sinais (backtrace e analise de 
 * registradores). Todo este arquivo eh dependente de arquitetura.
 * Varios ifdef's para determinar a implementacao correta.
 */

#if defined(MIPSEL) || defined(MIPSEB)

/* 
 * Arquitetura MIPS. Vamos supor que eh usada a uclibc, ao inves
 * da glibc. Isto implica na falta da funcao backtrace. Temos que 
 * implementa-la. Mas antes, temos que definir algumas constantes.
 */

/**
 * Padrão de memória que marca o tamanho da pilha.
 */
#define SW_STACK_SIZE		0x27bd0000

/**
 * Padrão de memória que marca o endereço de retorno
 * da função.
 */
#define SW_RETURN_ADDRESS	0xafbf0000

/**
 * Padrão de memória que marca o fim da pilha.
 */
#define STACK_END		0x3c1c0000

/**
 * Executa um backtrace. Deve ser chamado dentro de um tratador de
 * sinal.
 * @param buffer Buffer que armazena os endereços do backtrace.
 * @param size Número máximo de entradas no buffer.
 * @param uc Contexto de execução.
 * @return 0, em caso de falha. Número de entradas retornadas, 
 * caso contrário.
 */
int sigbacktrace(void ** buffer, int size, ucontext_t const *uc) {

	unsigned long * addr;
	unsigned long * ra;
	unsigned long * sp;
	unsigned long * pc;
	size_t ra_offset;
	size_t stack_size;
	int depth;

	if (!size) return(0);
	if (!buffer || size < 0) return(-EINVAL);

	pc = (unsigned long *) (unsigned long)uc->uc_mcontext.pc;
	ra = (unsigned long *) (unsigned long)uc->uc_mcontext.gregs[31];
	sp = (unsigned long *) (unsigned long)uc->uc_mcontext.gregs[29];

	buffer[0] = pc;

	if (size == 1) return(1);

	ra_offset = stack_size = 0;

	for (addr = pc; !ra_offset || !stack_size; --addr) {

		switch (* addr & 0xffff0000) {

			case SW_STACK_SIZE:
				stack_size = abs((short) (* addr & 0xffff));
				goto __out_of_loop;
			case SW_RETURN_ADDRESS:
				ra_offset = (short) (* addr & 0xffff);
				break ;
			case STACK_END:
				goto __out_of_loop;
			default:
				break ;
		}
	}

__out_of_loop:

	if (ra_offset)
		ra = * (unsigned long **) ((unsigned long) sp + ra_offset);
	if (stack_size)
		sp = (unsigned long *) ((unsigned long) sp + stack_size);

	for (depth = 1; depth < size && ra; ++depth) {

		buffer[depth] = ra;
		
		ra_offset = 0;
		stack_size = 0;

		for (addr = ra; !ra_offset || !stack_size; --addr) {

			switch (* addr & 0xffff0000) {

				case SW_STACK_SIZE:
					stack_size = abs((short) (* addr & 0xffff));
					break ;
				case SW_RETURN_ADDRESS:
					ra_offset = (short) (* addr & 0xffff);
					break ;
				case STACK_END:
					return(depth + 1);
				default:
					break ;
			}
		}

		ra = * (unsigned long **) ((unsigned long) sp + ra_offset);
		sp = (unsigned long *) ((unsigned long) sp + stack_size);
	}

	return(depth);
}

/**
 * Realiza um backtrace. Não deve ser chamado em tratadores de sinal.
 * @param buffer Buffer que armazena os endereços do backtrace.
 * @param size Número máximo de entradas no backtrace.
 * @return 0, em caso de falha. Número de entradas retornadas, 
 * caso contrário.
 */
int backtrace(void ** buffer, int size) {

	unsigned long * addr;
	unsigned long * ra;
	unsigned long * sp;
	size_t ra_offset;
	size_t stack_size;
	int depth;

	if (!size) return(0);
	if (!buffer || size < 0) return(-EINVAL);

	__asm__ __volatile__ (
		"move %0, $ra\n"
		"move %1, $sp\n"
		: "=r"(ra), "=r"(sp)
	);

	stack_size = 0;

	for (addr = (unsigned long *) backtrace; !stack_size; ++addr) {

		if ((* addr & 0xffff0000) == SW_STACK_SIZE)
			stack_size = abs((short) (* addr & 0xffff));
		else if (* addr == 0x03e00008) break ;
	}

	sp = (unsigned long *) ((unsigned long) sp + stack_size);

	for (depth = 0; depth < size && ra; ++depth) {

		buffer[depth] = ra;
		
		ra_offset = 0;
		stack_size = 0;

		for (addr = ra; !ra_offset || !stack_size; --addr) {

			switch (* addr & 0xffff0000) {

				case SW_STACK_SIZE:
					stack_size = abs((short) (* addr & 0xffff));
					break ;
				case SW_RETURN_ADDRESS:
					ra_offset = (short) (* addr & 0xffff);
					break ;
				case STACK_END:
					return(depth + 1);
				default:
					break ;
			}
		}

		ra = * (unsigned long **) ((unsigned long) sp + ra_offset);
		sp = (unsigned long *) ((unsigned long) sp + stack_size);
	}

	return(depth);
}

/**
 * Faz um dump do estado dos registradores, baseado no contexto
 * passado para o tratador de sinais.
 * @param output Descritor de arquivo para a saída.
 * @param uc Contexto antes do sinal.
 * @return Nada.
 */
void dump_regs(FILE * output, ucontext_t * uc) {

	int i;

	fprintf(output, "\nGeneral registers:\n");
	for (i = 0; i < NGREG; i++)
		fprintf(output, "\t- $%d: %08X\n", i, (unsigned int) (uc->uc_mcontext.gregs[i] & 0xffffffff));

	fprintf(output, "\nFloat-point registers:\n");
	for (i = 0; i < NFPREG; i++)
		fprintf(output, "\t- $%i: %e\n", i, uc->uc_mcontext.fpregs.fp_r.fp_dregs[i]);

	fprintf(output, "\nSpecial Registers:\n");
	fprintf(output, "\t- PC: %p\n", (void *) ((unsigned int) (uc->uc_mcontext.pc & 0xffffffff)));
}

#else

/*
 * Arquitetura x86 ou outra. Vamos supor o uso da glibc.
 */

/**
 * Faz um dump do estado dos registradores, baseado no contexto
 * passado para o tratador de sinais.
 * @param output Descritor de arquivo para a saída.
 * @param uc Contexto antes do sinal.
 * @return Nada.
 */
void dump_regs(FILE * output, ucontext_t * uc) {

	int i;

//	fprintf(output, "\nGeneral registers:\n");
//	for (i = 0; i < NGREG; i++)
//		fprintf(output, "\t- $%d: %08X\n", i, (unsigned int) (uc->uc_mcontext.gregs[i] & 0xffffffff));
//
//	fprintf(output, "\nSpecial Registers:\n");
//	fprintf(output, "\t- PC: %08X\n", (unsigned int) (uc->uc_mcontext.gregs[14] & 0xffffffff));
}


#endif

/**
 * Tenta resolver o nome de uma função a partir de um endereço de memória,
 * e imprime uma linha do backtrace.
 * @param output Descritor do arquivo de saída.
 * @param self_hnd Não usado.
 * @param address Endereço a ser resolvido.
 * @return Nada.
 */
void resolve_and_print_symbol(FILE * output, void * self_hnd, void * address) {

	Dl_info info;
	unsigned long offset;

        dladdr(address, & info);
	offset = ((unsigned long) address) - ((unsigned long) info.dli_saddr);
	fprintf(output, "\t- %s(%s+0x%lX) [%p]\n", info.dli_fname, info.dli_sname, offset, address);
}

/**
 * Tratador de sinais não esperados. Esses sinais geram backtrace e dump.
 * @param signum Número do sinal recebido.
 * @param siginfo Informações sobre o sinal recebido.
 * @param ucp Ponteiro para a estrutura de contexto.
 * @return Nada.
 */
void bad_signal_handler(int signum, siginfo_t * siginfo, void * ucp) {

	FILE * err;
	void * bt[30];
	int i;
	int levels;
	void * self_hnd;

	if (daemonize) {

		err = fopen("proto.crash", "w");
	}
	else {

		err = stderr;
	}

	fprintf(err, "This is a failure report from SLSP (built in %s at %s). \n"
			"Please send this complete output to dpassos@ic.uff.br for support.\n\n",
			__DATE__, __TIME__);

	fprintf(err, "SLSP was started at %s and crashed at %s.\n\n", start_date, get_current_time(NULL));
	fprintf(err, "The received signal was %d at %p. Signal Code: %d.\n\n", signum, siginfo->si_addr, siginfo->si_code);

	self_hnd = dlopen(NULL, RTLD_LAZY);

	levels = sigbacktrace(bt, 30, (ucontext_t *) ucp);
	fprintf(err, "The backtrace for the failure is:\n");
	for (i = 0; i < levels; i++) {

		resolve_and_print_symbol(err, self_hnd, bt[i]);
	}

	dlclose(self_hnd);

	fprintf(err, "\nAdditional (potencially usefull information):\n"
		"\t- si_int = %d\n"
		"\t- si_prt = %p\n", siginfo->si_int, siginfo->si_ptr);

	dump_regs(err, (ucontext_t *) ucp);

	if (daemonize) fclose(err);

//	p_finalize();

	exit(1);
}

/**
 * Tratador de sinal para sinais esperados (interrupção do usuário).
 * @param signum Número do sinal recebido.
 * @return Nada.
 */
void good_signal_handler(int signum) {
	 
	/* Parar as threads. */
	
	run = 0;
	
	/* Liberar o processo principal para o encerramento. */
	
	sem_post(&finalize_mutex);
}

/**
 * Instala os tratadores de sinal.
 * @return Nada.
 */
void install_handler() {

	struct sigaction sa;

	memset(& sa, 0, sizeof(sa));
	sa.sa_sigaction = bad_signal_handler;
	sa.sa_flags = SA_SIGINFO;

	/*
	 * Sinais de erros.
	 */

	sigaction(SIGILL, & sa, NULL);
	sigaction(SIGSEGV, & sa, NULL);
	sigaction(SIGFPE, & sa, NULL);
	sigaction(SIGABRT, & sa, NULL);
	sigaction(SIGBUS, & sa, NULL);

	/*
	 * Sinais de termino normal.
	 */

	signal(SIGINT, good_signal_handler);
	signal(SIGTERM, good_signal_handler);

}

