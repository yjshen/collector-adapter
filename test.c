/*************************************************************************
        > File Name: test.c
        > Author: 
        > Mail: 
        > Created Time: Tue 19 May 2015 09:15:42 AM PDT
 ************************************************************************/
#include "common.h"
#include "receiver.h"
#include "conf.h"
#include "load.h"
#include "log.h"

#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

int getSleepTime(int rate){
    // 1300B in a package
    // us = 1300 * 1000 * 1000 / * 1024 * 1024 * MB
    return 1240 / rate; 
}

int main(int argc, char ** args) {
    
    configure();
    test_loadData();
    if(initClient() == FALSE ){
        return -1;
    }
     testData * data = getData();
    buffer_s* sendDataBuf = fillNetflowData(data);
    
    int initRate = 140;      // 10MB/s
   
    struct timeval start, end;
    int sleepTime = getSleepTime(initRate);         // us
    
    while (1) {
        long t = end.tv_usec - start.tv_usec;
        if (end.tv_sec == start.tv_sec && t >0 && t < sleepTime) {
            usleep(sleepTime - t);
        }
         gettimeofday(&start,NULL);
        if(runClient(sendDataBuf)){
            data = getData();
            sendDataBuf = fillNetflowData(data);
        }
          gettimeofday(&end,NULL);
    }
    return 0;
}

