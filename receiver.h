/*************************************************************************
        > File Name: receiver.h
        > Author: 
        > Mail: 
        > Created Time: Tue 19 May 2015 09:15:25 AM PDT
 ************************************************************************/

#ifndef _RECEIVER_H
#define _RECEIVER_H

#include "common.h"

#ifdef TEST
#include "load.h"
#include "log.h"
#else
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#endif

typedef struct buffer_s {
    u_char* buff;
    uint32_t bufflen;
    uint32_t buffMaxLen;
} buffer_s;

BOOLEAN initClient(void) ;
BOOLEAN runClient(struct buffer_s* data);

#ifdef TEST
struct buffer_s* fillNetflowData(testData* eth_hdr);
#else
struct buffer_s* fillNetflowData(struct ether_hdr* eth_hdr);
#endif

#endif

