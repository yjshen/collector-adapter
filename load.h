/*************************************************************************
        > File Name: load.h
        > Author: 
        > Mail: 
        > Created Time: Wed 20 May 2015 08:06:45 AM PDT
 ************************************************************************/

#ifndef _LOAD_H
#define _LOAD_H

#include <stdint.h>

#define BASEINC 1000000

typedef struct {
    char data[1500];
    uint16_t length;
} testData;

struct {
    testData* datalist;
    uint32_t maxNum;
    uint32_t totalNum;
    uint32_t currId;
    uint32_t cycleCount;
} testDataList;

void test_loadData();
testData* getData();

#endif
