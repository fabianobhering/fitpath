/**
 * @file
 * @author Diego Passos <dpassos@ic.uff.br>
 * @version 1.0
 *
 * @section DESCRIPTION
 *
 * Módulo que implementa o cálculo da métrica MARA (e suas variações).
 */


/*
 * Arquivo de implementacao das funcoes relacionadas a metrica MARA (e suas variacoes).
 */

#include "mara.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "util.h"
#include "rule.h"

/*
 * Tipos de dados.
 */

/**
 * Tipo de dados que define a estrutura de um nó da tabela de valores dos
 * coeficientes a e b.
 */
typedef struct LP {

	/**
	 * Tamanho do pacote referente a esta entrada.
	 */
	unsigned short  size;

	/**
	 * Valor do coeficiente a.
	 */
	double          a;

	/**
	 * Valor do coeficiente b.
	 */
	double          b;

	/**
	 * Ponteiro para o próximo nó.
	 */
	struct LP       * next;

	/**
	 * Ponteiro para o nó anterior.
	 */
	struct LP       * prev;

} entryListPointer;

/**
 * Estrutura que guarda as estimativas de probabilidade de entrega entre
 * dois nós.
 */
typedef struct {

	/**
	 * Probabilidade de entrega do nó local para um determinado vizinho.
	 */
	float		nlq;

	/**
	 * Probabilidade de entrega do nó vizinho para o nó local.
	 */
	float		lq;
} t_lq_nlq_pair;

/*
 * Variaveis Globais.
 */

/**
 * Tabela de conversão de PER. É um vetor de ponteiros para listas 
 * duplamente encadeadas contendo os valores dos coeficientes a e b.
 * para cada tamanho de pacote. Cada entrada do vetor corresponde a 
 * uma taxa de transmissão disponível na camada de enlace.
 * @sa entryListPointer
 */
entryListPointer * perTableIndex[12];

/*
 * Rotinas de Leitura do arquivo de tabela.
 */

/**
 * Insere uma entrada na tabela de conversão de PER.
 * @param rateIndex Índice relativo à taxa de transmissão.
 * @param pktSize Tamanho de pacote considerado.
 * @param a Valor do coeficiente a.
 * @param b Valor do coeficiente b.
 * @return Nada.
 */
void table_insert_entry(unsigned char rateIndex, unsigned short pktSize, double a, double b) {

        entryListPointer * newListPointer;

        if (!perTableIndex[rateIndex]) {

                newListPointer = p_alloc(sizeof(entryListPointer));
                newListPointer->size = pktSize;
                newListPointer->a = a;
                newListPointer->b = b;
                newListPointer->next = NULL;
                newListPointer->prev = NULL;
                perTableIndex[rateIndex] = newListPointer;
        }

        if (perTableIndex[rateIndex]->size > pktSize) {

                newListPointer = p_alloc(sizeof(entryListPointer));
                newListPointer->size = pktSize;
                newListPointer->a = a;
                newListPointer->b = b;
                newListPointer->next = NULL;
                newListPointer->prev = perTableIndex[rateIndex];
                perTableIndex[rateIndex]->next = newListPointer;
                perTableIndex[rateIndex] = newListPointer;
        }
}

/**
 * Rotina de inicialização do módulo.
 * @param table_filename String contendo o caminho para o arquivo que
 * descreve os valores dos coeficientes a e b para várias taxas e 
 * tamanhos de pacote.
 * @return Nada.
 */
int mara_init(char * table_filename) {

	FILE * table_file;
	unsigned char rateIndex;
	double a, b;
	unsigned short pktSize;
	int i;


	table_file = fopen(table_filename, "r");
	if (table_file == NULL) {

		fprintf(stderr, "Unable to open perTable file:\n");
		perror(NULL);
		return(-1);
	}

        for (i = 0; i < 12; i++)
                perTableIndex[i] = NULL;

        while(!feof(table_file)) {

                if (fscanf(table_file, "%hhu,%hu,%lf,%lf\n", & rateIndex, & pktSize, & a, & b)  < 4) {

                        fprintf(stderr, "Invalid data in perTable.");
                        return(-1);
                }

                table_insert_entry(rateIndex, pktSize, a, b);
        }

        fclose(table_file);
        return(0);
}

/**
 * Rotina de inicialização da variação MARA-P.
 * @return Nada.
 */
void mararp_init() {

	char cmd[100];
	int i;

	/*
	 * Carregar o modulo necessario.
	 */

	sprintf(cmd, "%s %s", insmod_path, length_mod_path);
	system(cmd);

	/*
	 * Criar as regras de firewall.
	 */


	sprintf(cmd, "%s -A PREROUTING -t mangle -m length --length 0:%d -j MARK --set-mark 1", iptables_path, rateClasses[0]);
	system(cmd);
	sprintf(cmd, "%s -A OUTPUT -t mangle -m length --length 0:%d -j MARK --set-mark 1", iptables_path, rateClasses[0]);
	system(cmd);
	for (i = 1; i < nRateClasses; i++) {

		sprintf(cmd, "%s -A PREROUTING -t mangle -m length --length %d:%d -j MARK --set-mark %i", iptables_path, rateClasses[i-1], rateClasses[i], i + 1);
		system(cmd);
		sprintf(cmd, "%s -A OUTPUT -t mangle -m length --length %d:%d -j MARK --set-mark %i", iptables_path, rateClasses[i-1], rateClasses[i], i + 1);
		system(cmd);
	}

	/*
	 * Criar as regras de roteamento.
	 */

	for (i = 1; i <= nRateClasses; i++) {

		rule_add(i, i);
/*
 * Codigo antigo. Não remover, por enquanto.
 */
#if 0
		sprintf(cmd, "%s rule add fwmark %d table %d", ip_path, i, i);
		system(cmd);
#endif
	}
}

/*
 * Rotina de finalizacao.
 */

/**
 * Rotina de encerramento do módulo. Libera memória dinamicamente alocada.
 * @return Nada.
 */
void mara_finalize() {

        entryListPointer * walker;
        int i;

        for(i = 0; i < 12; i++) {

                while(perTableIndex[i]) {

                        walker = perTableIndex[i]->prev;
                        p_free(perTableIndex[i]);
                        perTableIndex[i] = walker;
                }
        }
}

/**
 * Rotina de encerramento para a variação MARA-P. Desfaz as regras de firewall.
 * @return Nada.
 */
void mararp_finalize() {

	char cmd[100];
	int i;

	/*
	 * Apagar as regras de firewall.
	 */

	sprintf(cmd, "%s -D PREROUTING -t mangle -m length --length 0:%d -j MARK --set-mark 1", iptables_path, rateClasses[0]);
	system(cmd);
	sprintf(cmd, "%s -D OUTPUT -t mangle -m length --length 0:%d -j MARK --set-mark 1", iptables_path, rateClasses[0]);
	system(cmd);
	for (i = 1; i < nRateClasses; i++) {

		sprintf(cmd, "%s -D PREROUTING -t mangle -m length --length %d:%d -j MARK --set-mark %i", iptables_path, rateClasses[i-1], rateClasses[i], i + 1);
		system(cmd);
		sprintf(cmd, "%s -D OUTPUT -t mangle -m length --length %d:%d -j MARK --set-mark %i", iptables_path, rateClasses[i-1], rateClasses[i], i + 1);
		system(cmd);
	}

	/*
	 * Apagar as regras de roteamento.
	 */

	for (i = 1; i <= nRateClasses; i++) {

		sprintf(cmd, "%s rule del fwmark %d table %d", ip_path, i, i);
		system(cmd);
	}

	sprintf(cmd, "%s %s", rmmod_path, loseExt(steam(length_mod_path)));
	system(cmd);

}

/*
 * Rotinas matematicas.
 */

/**
 * Cálculo da função erro.
 * @param x argumento da função erro.
 * @return Valor da função no ponto x.
 */
static inline double
__erf__(double x)
{
        double z, ax, ax2, erf;
        static double a1 = .254829592;
        static double a2 = -.284496736;
        static double a3 = 1.421413741;
        static double a4 = -1.453152027;
        static double a5 = 1.061405429;
        static double scale = .3275911000;
        static double c1 = 1.1283791671;
        static double c2 = 0.6666666666;
        static double c3 = 0.2666666666;

        ax=fabs(x);
        ax2= ax*ax;

        if (ax > 0.1) {
                z = 1./(1.+scale*ax);
                erf = 1. - z*exp(-ax2)*(a1 +z*(a2 +z*(a3 +z*(a4 +z*a5))));
        }
        else {
                erf = c1 * exp(-ax2) * (ax * (1 + ax2 * (c2 + c3*ax2)));
        }

        if (x < 0.0)  return(-erf);

        return(erf);
}

/**
 * Cálculo da função erro inversa.
 * @param x Argumento para a função erro inversa.
 * @return Valor da função erro inversa para x.
 */
static inline double
erfinv(double x)
{
        /*
           Source: This routine was derived (using f2c) from the
                   FORTRAN subroutine MERFI found in
                   ACM Algorithm 602 obtained from netlib.

                   MDNRIS code contains the 1978 Copyright
                   by IMSL, INC. .  Since MERFI has been
                   submitted to netlib, it may be used with
                   the restriction that it may only be
                   used for noncommercial purposes and that
                   IMSL be acknowledged as the copyright-holder
                   of the code.
        */



        /* Initialized data */
        static double a1 = -.5751703;
        static double a2 = -1.896513;
        static double a3 = -.05496261;
        static double b0 = -.113773;
        static double b1 = -3.293474;
        static double b2 = -2.374996;
        static double b3 = -1.187515;
        static double c0 = -.1146666;
        static double c1 = -.1314774;
        static double c2 = -.2368201;
        static double c3 = .05073975;
        static double d0 = -44.27977;
        static double d1 = 21.98546;
        static double d2 = -7.586103;
        static double e0 = -.05668422;
        static double e1 = .3937021;
        static double e2 = -.3166501;
        static double e3 = .06208963;
        static double f0 = -6.266786;
        static double f1 = 4.666263;
        static double f2 = -2.962883;
        static double g0 = 1.851159e-4;
        static double g1 = -.002028152;
        static double g2 = -.1498384;
        static double g3 = .01078639;
        static double h0 = .09952975;
        static double h1 = .5211733;
        static double h2 = -.06888301;

        /* Local variables */
        double f, w, z, sigma, z2, sd, wi, sn;

        /* determine sign of x */
        if (x > 0)
                sigma = 1.0;
        else
                sigma = -1.0;

        /* Note: -1.0 < x < 1.0 */

        z = fabs(x);

        /* z between 0.0 and 0.85, approx. f by a
           rational function in z  */

        if (z < 0.85) {
                z2 = z * z;
                f = z + z * (b0 + a1 * z2 / (b1 + z2 + a2
                    / (b2 + z2 + a3 / (b3 + z2))));

        /* z greater than 0.85 */
        } else {

                /* reduced argument is in (0.85,1.0),
                   obtain the transformed variable */

                w = sqrt(-(double)(log(1.0 - z * z)));

                if (w < 2.5) {

                        sn = ((c3 * w + c2) * w + c1) * w;
                        sd = ((w + d2) * w + d1) * w + d0;
                        f = w + w * (c0 + sn / sd);
                }
                else if (w < 4.0) {
                        sn = ((e3 * w + e2) * w + e1) * w;
                        sd = ((w + f2) * w + f1) * w + f0;
                        f = w + w * (e0 + sn / sd);
                }
                else {
                        wi = 1.0 / w;
                        sn = ((g3 * wi + g2) * wi + g1) * wi;
                        sd = ((wi + h2) * wi + h1) * wi + h0;
                        f = w + w * (g0 + sn / sd);
                }
        }
        return(sigma * f);
}

/*
 * Rotinas de consulta a dados da tabela.
 */

/**
 * Interpola o valor de uma função em um ponto através de uma reta passando
 * por um ponto anterior e um ponto posterior.
 * @param x1 Abscissa do ponto anterior.
 * @param x2 Abscissa do ponto posterior.
 * @param y1 Valor da função em x1.
 * @param y2 Valor da função em x2.
 * @param x_n Abscissa na qual se deseja calcular o valor da função.
 * @return Valor aproximado da função em x_n.
 */
float table_interpolate(float x1, float x2, float y1, float y2, float x_n) {

        float ret;

        if (x1 == x2)
                return((y1 + y2)/2);
        ret = (y2 - y1)/(x2 - x1);
        ret *= (x_n - x1);
        ret += y1;

        return(ret);
}

/**
 * Consulta a tabela de coeficientes para conversão de PER e estima o SNR 
 * para um dado valor de PER, uma taxa de transmissão e um tamanho de pacote.
 * @param PER Valor de PER.
 * @param rateIndex Índice da taxa de transmissão.
 * @param pkt_size Tamanho do pacote.
 * @return Estimativa do valor de SNR para os parâmetros de entrada.
 */
float table_get_snr(float PER, int rateIndex, int pkt_size) {

        entryListPointer * lastGreaterPktSize = NULL;
        entryListPointer * nextLowerPktSize = NULL;
        entryListPointer * LPwalker = NULL;
        float lowerPktInterSNR;
        float higherPktInterSNR;
        int lowerPktSize;
        int higherPktSize;
        double erf_inv_cache;

        /*
         * First, let's do some sanity checks.
         */

        if (PER < 0.0 || PER > 1.0) return(-99999.0);
        if (pkt_size < 0 || pkt_size > 1500) return(-999999.0);
        if (rateIndex < 0 || rateIndex > 11) return(-999999.0);

/**
 * Limiar de precisão para o valor de PER. Utilizado para evitar os pontos
 * extremos das curvas.
 */
#define PRECISION_LIMIT 0.0001

        if (PER == 1.0) PER -= PRECISION_LIMIT;
        if (PER == 0.0) PER += PRECISION_LIMIT;

        erf_inv_cache = M_SQRT2 * erfinv(1.0 - 2.0 * PER);

        /*
         * Find the initial point of this rateIndex in the table.
         */

        LPwalker = perTableIndex[rateIndex];

        if (!LPwalker) {

                /*
                 * No entry has been found for this rateIndex. This
                 * also might mean that the table is not correctly 
                 * sorted. Anyway, that's an error condition.
                 */

		/*
		 * TODO: adaptar ao metodo de tratamento de erros do resto do
		 * programa.
		 */

                fprintf(stderr, "Unable to find rateIndex %d.\n", rateIndex);
                exit(1);
        }

        /*
         * Here, we suppose each group of the table is sorted by pktSize.
         * Therefore, we have to find the pktSizes greater and lower than
         * 'pkt_size'.
         */

        while(LPwalker) {

                if (LPwalker->size == pkt_size) {

                        /* 
                         * We have to go to the last entry with this packet size,
                         * to facilitate the further processing.
                         */

                        lastGreaterPktSize = LPwalker;
                        nextLowerPktSize = LPwalker;
                        break ;
                }

                if (LPwalker->size > pkt_size) {

                        nextLowerPktSize = LPwalker;
                        break ;
                }

                lastGreaterPktSize = LPwalker;
                LPwalker = LPwalker->prev;
        }

        if (nextLowerPktSize) {

                lowerPktInterSNR = nextLowerPktSize->a + nextLowerPktSize->b * erf_inv_cache;
                lowerPktSize = nextLowerPktSize->size;

                if (nextLowerPktSize == lastGreaterPktSize) {

                        /* 
                         * In this case, we have all the information we need.
                         */

                        return(lowerPktInterSNR);
                }
        }
        else {

                /* 
                 * No, we haven't. We don't have information enougth to
                 * perform an interpolation. So, let's be pessimistic.
                 * We will consider packet lowerPktSize as 0 and SNR as 
                 * the lower possible (-1). TODO: review this choice.
                 */

                lowerPktSize = 0;
                lowerPktInterSNR = -1.0;
        }

        if (lastGreaterPktSize) {

                higherPktInterSNR = lastGreaterPktSize->a + lastGreaterPktSize->b * erf_inv_cache;
                higherPktSize = lastGreaterPktSize->size;
        }
        else {

                return(lowerPktInterSNR);
        }

        /* 
         * If we reached this point, we must interpolate the interpolated values.
         */

	return(table_interpolate(lowerPktSize, higherPktSize, lowerPktInterSNR, higherPktInterSNR, pkt_size));
}

/**
 * Consulta a tabela de coeficientes para conversão de PER e estima o PER
 * para um dado valor de SNR, uma taxa de transmissão e um tamanho de pacote.
 * @param SNR Valor de SNR.
 * @param rateIndex Índice da taxa de transmissão.
 * @param pkt_size Tamanho do pacote.
 * @return Estimativa do valor de PER para os parâmetros de entrada.
 */
float table_get_PER(float SNR, int rateIndex, int pkt_size) {

        entryListPointer * LPwalker = NULL;
        entryListPointer * lastGreaterPktSize = NULL;
        entryListPointer * nextLowerPktSize = NULL;
        float lowerPktInterPER;
        float higherPktInterPER;
        int lowerPktSize;
        int higherPktSize;

        /*
         * First, let's do some sanity checks.
         */

        if (pkt_size < 0 || pkt_size > 1500) return(-999999.0);
        if (rateIndex < 0 || rateIndex > 11) return(-999999.0);

        /*
         * Find the initial point of this rateIndex in the table.
         */

        LPwalker = perTableIndex[rateIndex];

        if (!LPwalker) {

                /*
                 * No entry has been found for this rateIndex. This
                 * also might mean that the table is not correctly 
                 * sorted. Anyway, that's an error condition.
                 */

                fprintf(stderr, "Unable to find rateIndex %d.\n", rateIndex);
                exit(1);
        }

        /*
         * Here, we suppose each group of the table is sorted by pktSize.
         * Therefore, we have to find the pktSizes greater and lower than
         * 'pkt_size'.
         */

        while(LPwalker) {

                if (LPwalker->size == pkt_size) {

                        /* 
                         * We have to go to the last entry with this packet size,
                         * to facilitate the further processing.
                         */

                        lastGreaterPktSize = LPwalker;
                        nextLowerPktSize = LPwalker;
                        break ;
                }

                if (LPwalker->size > pkt_size) {

                        nextLowerPktSize = LPwalker;
                        break ;
                }

                lastGreaterPktSize = LPwalker;
                LPwalker = LPwalker->prev;
        }

        if (nextLowerPktSize) {

                lowerPktInterPER = (1.0 - __erf__((SNR - nextLowerPktSize->a)/(M_SQRT2 * nextLowerPktSize->b)))/2.0;
                lowerPktSize = nextLowerPktSize->size;

                if (nextLowerPktSize == lastGreaterPktSize) {

                        /* 
                         * In this case, we have all the information we need.
                         */

                        return(lowerPktInterPER);
                }
        }
        else {

                /* 
                 * No, we haven't. We don't have information enougth to
                 * perform an interpolation. So, let's be pessimistic.
                 * We will consider packet lowerPktSize as 0 and PER as 
                 * the highest possible (1). TODO: review this choice.
                 */

                lowerPktSize = 0;
                lowerPktInterPER = 1.0;
        }

        if (lastGreaterPktSize) {

                higherPktInterPER = (1.0 - __erf__((SNR - lastGreaterPktSize->a)/(M_SQRT2 * lastGreaterPktSize->b)))/2.0;
                higherPktSize = lastGreaterPktSize->size;
        }
        else {

                return(lowerPktInterPER);
        }

        /* 
         * If we reached this point, we must interpolate the interpolated values.
         */


        return(table_interpolate(lowerPktSize, higherPktSize, lowerPktInterPER, higherPktInterPER, pkt_size));
}

/*
 * Rotinas de calculo de custo.
 */

/**
 * Escolhe o melhor par de probabilidades de entrega para o posterior
 * cálculo do SNR.
 * @param rate1_lq Probabilidade de entrega do vizinho para o nó na taxa de transmissão 1.
 * @param rate1_nlq Probabilidade de entrega do nó para o vizinho na taxa de transmissão 1.
 * @param rate2_lq Probabilidade de entrega do vizinho para o nó na taxa de transmissão 2.
 * @param rate2_nlq Probabilidade de entrega do nó para o vizinho na taxa de transmissão 2.
 * @param rate3_lq Probabilidade de entrega do vizinho para o nó na taxa de transmissão 3.
 * @param rate3_nlq Probabilidade de entrega do nó para o vizinho na taxa de transmissão 3.
 * @param rate4_lq Probabilidade de entrega do vizinho para o nó na taxa de transmissão 4.
 * @param rate4_nlq Probabilidade de entrega do nó para o vizinho na taxa de transmissão 4.
 * @param best_lq Guarda o valor escolhido para a melhor probabilidade de entrega do vizinho 
 * para o nó.
 * @param best_nlq Guarda o valor escolhido para a melhor probabilidade de entrega do nó para o 
 * vizinho.
 * @param bestRate Guarda a taxa de transmissão associada às probabilidades escolhidas.
 * @return Nada.
 */
void mara_get_best_lq_nlq_pair(float rate1_lq, float rate1_nlq, float rate2_lq, float rate2_nlq,
				float rate3_lq, float rate3_nlq, float rate4_lq, float rate4_nlq,
				float * best_lq, float * best_nlq, int * bestRate) {

	t_lq_nlq_pair lq[4];
	int i;

#define LIMIAR 0.25

	lq[0].lq = rate1_lq;
	lq[0].nlq = rate1_nlq;
	lq[1].lq = rate2_lq;
	lq[1].nlq = rate2_nlq;
	lq[2].lq = rate3_lq;
	lq[2].nlq = rate3_nlq;
	lq[3].lq = rate4_lq;
	lq[3].nlq = rate4_nlq;

#ifndef ORIGINAL
	for (i = nRateClasses - 1; i >= 0; i--) {

		* best_nlq = lq[i].nlq;
		* best_lq = lq[i].lq;
		* bestRate = mcastRate2Rate[i];

		if (i == 0) break ;

		if (* best_lq <= LIMIAR) continue ;
		if (* best_nlq > LIMIAR || lq[i - 1].nlq >= 1.0) break ;
	}
#else
#define ABS(x)	(((x) < 0.0) ? -(x):(x))
	* best_nlq = lq[0].nlq;
	* best_lq = lq[0].lq;
	* bestRate = mcastRate2Rate[0];
	for (i = 1; i < nRateClasses; i++) {

		if (* best_nlq < LIMIAR) continue ;
		if (ABS((* best_nlq) - 0.5) > ABS((lq[i].nlq - 0.5))) {

			* best_nlq = lq[i].nlq;
			* best_lq = lq[i].lq;
			* bestRate = mcastRate2Rate[i];
		}
	}
#endif
	/*
	 * Sometimes, because of jitter and approximation problems, best_lq and best_nlq
	 * may be higher than 1.0. Let's garantee this is not the case.
	 */

	if (* best_lq > 1.0) * best_lq = 1.0;
	if (* best_nlq > 1.0) * best_nlq = 1.0;
}

/**
 * Calcula o valor do custo de um enlace, dados todos os parâmetros necessários.
 * @param nlq_prob Probabilidade de entrega do nó para o vizinho.
 * @param rateIndex Índice da taxa de transmissão na qual foi estimada a probabilidade
 * de entrega.
 * @param snr SNR entre os dois nós.
 * @param bestRate Guarda a melhor taxa de transmissão encontrada (associada ao 
 * custo computado).
 * @param pktSize Tamanho do pacote para o qual estamos computando a métrica.
 * @return Nada.
 */
float mara_basic_calc(float nlq_prob, int rateIndex, float snr, int * bestRate, int pktSize) {

	int i;
	float rate;
	float bestMetric, possibleMetric;
	float newLq;
	float newETX;

	/*
	 * Codigo em testes.
	 */

	int lastRate;

	if (rateIndex == 0) lastRate = 3;
	else if (rateIndex == 7) lastRate = 7;
	else if (rateIndex == 9) lastRate = 9;
	else lastRate = 11;

	/* Evaluate all possible rates to find the best metric. */

	* bestRate = -1;
	bestMetric = INFINITY;

	for (i = 0; i <= lastRate; i++) {

		newLq = 1.0 - table_get_PER(snr, i, pktSize);
		if (newLq == 0.0) continue ;

		newETX = 1.0/(newLq * nlq_prob);

		rate = net_get_rate_from_index(i);
		possibleMetric = ((newETX * pktSize * 8)/rate);

		if (possibleMetric <= bestMetric) {

			bestMetric = possibleMetric;
			* bestRate = i;
		}
	}

	return(bestMetric);
}

/**
 * Calcula o custo de um enlace com base nas estimativas feitas em 4 diferentes taxas de 
 * transmissão. A função também configura a interface para utilizar a taxa de transmissão 
 * escolhida.
 * @param rate1_lq Probabilidade de entrega do vizinho para o nó na taxa de transmissão 1.
 * @param rate1_nlq Probabilidade de entrega do nó para o vizinho na taxa de transmissão 1.
 * @param rate2_lq Probabilidade de entrega do vizinho para o nó na taxa de transmissão 2.
 * @param rate2_nlq Probabilidade de entrega do nó para o vizinho na taxa de transmissão 2.
 * @param rate3_lq Probabilidade de entrega do vizinho para o nó na taxa de transmissão 3.
 * @param rate3_nlq Probabilidade de entrega do nó para o vizinho na taxa de transmissão 3.
 * @param rate4_lq Probabilidade de entrega do vizinho para o nó na taxa de transmissão 4.
 * @param rate4_nlq Probabilidade de entrega do nó para o vizinho na taxa de transmissão 4.
 * @param probeSize Tamanho do pacote de probe utilizado para inferir as probabilidades de 
 * entrega.
 * @param ip Endereço IP do vizinho.
 * @return Custo do enlace.
 */
float mara_calc_cost(float rate1_lq, float rate1_nlq, float rate2_lq, float rate2_nlq, 
			float rate3_lq, float rate3_nlq, float rate4_lq, float rate4_nlq, 
			unsigned short probeSize, const t_ip_addr ip) {

	int rateIndex;
	int bestRate;
	float nlq_prob, lq_prob;
	float bestMetric;
	float snr;

	mara_get_best_lq_nlq_pair(rate1_lq, rate1_nlq, rate2_lq, rate2_nlq, rate3_lq, rate3_nlq, rate4_lq, rate4_nlq,
					& lq_prob, & nlq_prob, & rateIndex);

	if (lq_prob == 0.0) return(INFINITY);

	/* Estimate the SNR of the link. */

	snr = table_get_snr(1 - lq_prob, rateIndex, probeSize);

	/* Compute MARA. */

	bestMetric = mara_basic_calc(nlq_prob, rateIndex, snr, & bestRate, probeSize);

	net_set_ifce_rate(bestRate, ip);
	return(bestMetric);
}

/**
 * Calcula o custo de um enlace com base nas estimativas feitas em 4 diferentes taxas de 
 * transmissão. A função também configura a interface para utilizar a taxa de transmissão 
 * escolhida para cada classe de tamanho de pacote disponível.
 * @param rate1_lq Probabilidade de entrega do vizinho para o nó na taxa de transmissão 1.
 * @param rate1_nlq Probabilidade de entrega do nó para o vizinho na taxa de transmissão 1.
 * @param rate2_lq Probabilidade de entrega do vizinho para o nó na taxa de transmissão 2.
 * @param rate2_nlq Probabilidade de entrega do nó para o vizinho na taxa de transmissão 2.
 * @param rate3_lq Probabilidade de entrega do vizinho para o nó na taxa de transmissão 3.
 * @param rate3_nlq Probabilidade de entrega do nó para o vizinho na taxa de transmissão 3.
 * @param rate4_lq Probabilidade de entrega do vizinho para o nó na taxa de transmissão 4.
 * @param rate4_nlq Probabilidade de entrega do nó para o vizinho na taxa de transmissão 4.
 * @param probeSize Tamanho do pacote de probe utilizado para inferir as probabilidades de 
 * entrega.
 * @param ip Endereço IP do vizinho.
 * @return Custo do enlace.
 */
float marap_calc_cost(float rate1_lq, float rate1_nlq, float rate2_lq, float rate2_nlq, 
			float rate3_lq, float rate3_nlq, float rate4_lq, float rate4_nlq, 
			unsigned short probeSize, const t_ip_addr ip) {

	int rateIndex;
	int bestRate;
	float nlq_prob, lq_prob;
	float bestMetric;
	float snr;
	int i;

	mara_get_best_lq_nlq_pair(rate1_lq, rate1_nlq, rate2_lq, rate2_nlq, rate3_lq, rate3_nlq, rate4_lq, rate4_nlq,
					& lq_prob, & nlq_prob, & rateIndex);

	if (lq_prob == 0.0) return(INFINITY);

	/* Estimate the SNR of the link. */

	snr = table_get_snr(1 - lq_prob, rateIndex, probeSize);

	/* Compute MARA. */

	for (i = 0; i < nRateClasses; i++) {

		/*
		 * Tirar o cabecalho IP.
		 */

		bestMetric = mara_basic_calc(nlq_prob, rateIndex, snr, & bestRate, rateClasses[i] - 20);
		net_set_ifce_rate_for_size(bestRate, ip, i);
	}

	return(bestMetric);
}

/**
 * Calcula o custo de um enlace para uma classe de tamanho específica com base nas estimativas 
 * feitas em 4 diferentes taxas de transmissão. A função também configura a interface para 
 * utilizar a taxa de transmissão escolhida para a classe de tamanho de pacote especificada.
 * @param rate1_lq Probabilidade de entrega do vizinho para o nó na taxa de transmissão 1.
 * @param rate1_nlq Probabilidade de entrega do nó para o vizinho na taxa de transmissão 1.
 * @param rate2_lq Probabilidade de entrega do vizinho para o nó na taxa de transmissão 2.
 * @param rate2_nlq Probabilidade de entrega do nó para o vizinho na taxa de transmissão 2.
 * @param rate3_lq Probabilidade de entrega do vizinho para o nó na taxa de transmissão 3.
 * @param rate3_nlq Probabilidade de entrega do nó para o vizinho na taxa de transmissão 3.
 * @param rate4_lq Probabilidade de entrega do vizinho para o nó na taxa de transmissão 4.
 * @param rate4_nlq Probabilidade de entrega do nó para o vizinho na taxa de transmissão 4.
 * @param probeSize Tamanho do pacote de probe utilizado para inferir as probabilidades de 
 * entrega.
 * @param toSizeIndex Índice da taxa de transmissão alvo.
 * @param ip Endereço IP do vizinho.
 * @return Custo do enlace.
 */
float mararp_calc_cost(float rate1_lq, float rate1_nlq, float rate2_lq, float rate2_nlq, 
			float rate3_lq, float rate3_nlq, float rate4_lq, float rate4_nlq, 
			unsigned short probeSize, unsigned short toSizeIndex, const t_ip_addr ip) {

	int rateIndex;
	int bestRate;
	float nlq_prob, lq_prob;
	float bestMetric;
	float snr;

	mara_get_best_lq_nlq_pair(rate1_lq, rate1_nlq, rate2_lq, rate2_nlq, rate3_lq, rate3_nlq, rate4_lq, rate4_nlq,
					& lq_prob, & nlq_prob, & rateIndex);

	if (lq_prob == 0.0) return(INFINITY);

	/* Estimate the SNR of the link. */

	snr = table_get_snr(1 - lq_prob, rateIndex, probeSize);

	/* Compute MARA. */

	bestMetric = mara_basic_calc(nlq_prob, rateIndex, snr, & bestRate, rateClasses[toSizeIndex] - 20);

	net_set_ifce_rate_for_size(bestRate, ip, toSizeIndex);
	return(bestMetric);
}

