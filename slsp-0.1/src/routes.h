#ifndef _ROUTES_H
#define _ROUTES_H 1

#ifndef PC
	#ifndef PRIMITIVE_TYPES
		#define PRIMITIVE_TYPES

		/**
		 * Inteiro sem sinal de 32 bits.
		 */
		typedef unsigned int __u32;

		/**
		 * Inteiro com sinal de 32 bits.
		 */
		typedef signed int __s32;

		/**
		 * Inteiro sem sinal de 16 bits.
		 */
		typedef unsigned short __u16;

		/**
		 * Inteiro com sinal de 16 bits.
		 */
		typedef signed short __s16;

		/**
		 * Inteiro sem sinal de 8 bits.
		 */
		typedef unsigned char __u8;

		/**
		 * Inteiro com sinal de 8 bits.
		 */
		typedef signed char __s8;
	#endif
#endif

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/route.h>
#include <linux/rtnetlink.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "topology.h"


/****************************************************************
 *                            Funções                           *
 ****************************************************************/

/**
 * Entrada em uma tabela de rotas.
 */
typedef struct {

	/**
	 * Endereço IP de destino.
	 */
	t_ip_addr		dest;

	/**
	 * Máscara de sub-rede (deve ser 255.255.255.255 para hosts).
	 */
	unsigned char		mask;

	/**
	 * Endereço IP para o gateway.
	 */
	t_ip_addr		gw;

	/**
	 * Valor inteiro para métrica.
	 */
	unsigned int		metric;

	/**
	 * Tabela de roteamento na qual será adicionada a rota.
	 */
	unsigned int		table;

	/**
	 * Ponteiro para a próxima rota na lista.
	 */
	struct t_rtentry *	next;
} t_rtentry;

/*
 * Codigo antigo. Não remover, por enquanto.
 */
#if 0
void init_routes();
#endif

void finalize_routes();
/*
 * Codigo antigo. Não remover, por enquanto.
 */
#if 0
void add_route(t_ip_addr, t_ip_addr, unsigned char, t_ip_addr);
void del_route(t_ip_addr, t_ip_addr, unsigned char, t_ip_addr);
#endif
void update_routes(t_ip_addr, t_graph_node *, int );
void update_subnet_routes(t_ip_addr, t_graph_node *, int );
t_rtentry * extract_route(t_ip_addr, int);
t_rtentry * extract_subnet_route(t_ip_addr, unsigned char, int);
void free_routes();
void free_subnet_routes();
void add_route(t_ip_addr, t_ip_addr, unsigned int, unsigned char, unsigned char, int);
void del_route(t_ip_addr, t_ip_addr, unsigned int, unsigned char, unsigned char, int);
/*
 * Codigo antigo. Não remover, por enquanto.
 */
#if 0
void add_route_MARARP(t_ip_addr, t_ip_addr, unsigned char, t_ip_addr, int);
void del_route_MARARP(t_ip_addr, t_ip_addr, unsigned char, t_ip_addr, int);
#endif

#endif
