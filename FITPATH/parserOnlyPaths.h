#ifndef __PARSER_H__
#define __PARSER_H__

#include "graph.h"

t_graph * parserParse(char * filename, t_list ** pathList, t_list ** flowTimeList, t_list ** packetTxTime);

#endif
