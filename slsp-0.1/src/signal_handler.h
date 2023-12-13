#ifndef __SIGNAL_HANDLER_H__
#define __SIGNAL_HANDLER_H__

#include <errno.h>
#include <ucontext.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>

#if defined(MIPSEL) || defined(MIPSEB)

#ifndef NFPREG 

/*
 * Versoes mais antigas da glibc apresentam definicoes diferentes.
 * Os proximos macros sao definidos para normalizar isso.
 */

#define NFPREG	(NGREG - 1)
#define gregs	gpregs
#define pc	gpregs[CTX_EPC]

#endif

#define abs(s)	((s) < 0 ? -(s) : (s))

int sigbacktrace(void ** , int , ucontext_t const *);
int backtrace(void ** , int );

#else

#include <execinfo.h>

/**
 * Para pc, a definição da função de backtrace não tem a variável de
 * contexto.
 */
#define sigbacktrace(buffer, size, uc)	backtrace(buffer, size)

#endif

void resolve_and_print_symbol(FILE * , void * , void * );
void bad_signal_handler(int , siginfo_t * , void * );
void good_signal_handler(int );
void dump_regs(FILE *, ucontext_t *);
void install_handler();

/*
 * TODO: a proxima declaracao eh implementada no protocolo.c. Nao
 * deveria estar aqui.
 */

void p_finalize();

#endif

