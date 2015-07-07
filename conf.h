/*************************************************************************
	> File Name: conf.h
	> Author: 
	> Mail: 
	> Created Time: Tue 19 May 2015 11:03:23 PM PDT
 ************************************************************************/

#ifndef _CONF_H
#define _CONF_H

#include "common.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

struct {
    struct sockaddr_in * masterIP;      //  current master list
    uint32_t masterNum;                      //  total master number
} masterList;

struct {
    uint32_t singleWaitSecond;
    uint32_t totalMaxTryNum;
    uint32_t receiverWaitSecond;
}netflowConf;

struct {
    char testLoadData[100];
    char testLoadTemp[100];
    char testLoadMix[100];
    char testLoadV5[100];
}netflowtest;

BOOLEAN configure(void);

#endif
