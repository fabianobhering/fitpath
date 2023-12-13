#ifndef __NL_H__
#define __NL_H_ 1

#include <asm/types.h>
#include <linux/rtnetlink.h>

/****************************************************
 *               Estruturas de Dados                *
 ****************************************************/

typedef struct {

	struct nlmsghdr 	n;
	struct rtmsg 		r;
	char   			buf[1024];
} t_route_req;

typedef struct 
{
	int			fd;
	struct sockaddr_nl	local;
	__u32			seq;
} nl_handle;

/****************************************************
 *               Prototipos de funcoes              *
 ****************************************************/

void rtnl_init();
void rtnl_fini();
int rtnl_send_request(struct nlmsghdr * );
void addattr32(struct nlmsghdr *, int, int, unsigned int);
void addattr_l(struct nlmsghdr *, int, int, const void *, int);

#endif

