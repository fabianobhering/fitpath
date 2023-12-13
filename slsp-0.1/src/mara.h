/*
 * Arquivo de cabecalho das funcoes relacionadas a metrica MARA (e suas 
 * variacoes).
 */

#ifndef __MARA_H__
#define __MARA_H__

#include "net.h"

/****************************************************************
 *                           Constantes                         *
 ****************************************************************/

/**
 * Valor da métrica infinita.
 */
#define INFINITY		((float) 999999999999.0)	// Infinito do algoritmo de caminho mínimo

int mara_init(char * );

void mararp_init();

void mara_finalize();

void mararp_finalize();

float mara_calc_cost(float, float, float, float, float, float, float, float, unsigned short, const t_ip_addr);

float marap_calc_cost(float, float, float, float, float, float, float, float, unsigned short, const t_ip_addr);

float mararp_calc_cost(float, float, float, float, float, float, float, float, unsigned short, unsigned short, const t_ip_addr);

#endif

