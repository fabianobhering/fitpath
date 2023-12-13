/* Funções de manipulação da rede (interfaces, envios de pacote...)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
	
#include "util.h"
#include "defs.h"

/**
 * Número de taxas de transmissões disponíveis na camada de enlace.
 */
#define MAX_RATES		12

/*
 * Variaveis exportadas.
 */

extern int mcastRate2Rate[];
extern int nRateClasses;
extern int rateClasses[];


/****************************************************************
 *                            Funções                           *
 ****************************************************************/


int get_ifce_index(char *);

t_ip_addr get_ifce_ipaddr(char *);

t_ip_addr get_ifce_bcastaddr(char *);

char * inetaddr(u_int32_t);

void create_send_sock(void);

void create_recv_sock(void);

void p_send(void *, size_t, t_ip_addr);

size_t p_recv(void *, t_ip_addr *);

void destroy_send_sock(void);

void destroy_recv_sock(void);

int net_load_pprs(void);

inline int net_unload_pprs(void);

int net_define_classes(void);

inline double net_get_rate_from_index(int rateIndex);

void net_set_ifce_rate(char newrate, const t_ip_addr ip);

void net_set_ifce_rate_for_size(char newrate, const t_ip_addr ip, int size_class);

