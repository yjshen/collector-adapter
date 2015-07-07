/*************************************************************************
	> File Name: utils.c
	> Author: 
	> Mail: 
	> Created Time: Wed 20 May 2015 08:06:54 AM PDT
 ************************************************************************/
#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>		//isblank()
#include <time.h>

//**********************************************************
//      Global Function Implement
//**********************************************************

static struct command_t {
    const  char msg_prefix[3];     // $$           result is  $$+2&192.1.1.1:1000&1.2.2.2.2:334
    const char inner_delim[2];    // ;
    const char outer_delim[2];    // &
} command = {  "$$", ";", "&" };

/*   delete left blank space   */
char * del_left_trim(char *str) {
    for (;*str != '\0' && isblank(*str) ; ++str);
    return str;
}

/*   delete left&&rigth blank space   */
char * del_both_trim(char * str) {
    char *p;
    char * szOutput;
    szOutput = del_left_trim(str);
    for (p = szOutput + strlen(szOutput) - 1; p >= szOutput && isblank(*p);
            --p);
    *(++p) = '\0';
    return szOutput;
}

void setAddress( struct sockaddr_in* add, char* ip, uint16_t port ){
    bzero(add, sizeof(struct sockaddr_in));
    add->sin_family = AF_INET;
    add->sin_port = htons(port);
    inet_pton(AF_INET, ip, &add->sin_addr);
    bzero(&add->sin_zero, sizeof(add->sin_zero));
}

char* getLongTime(char* timeBuff, uint8_t buffLen){
    time_t timer=time(NULL);
    strftime(timeBuff, buffLen, "%Y-%m-%d %H:%M:%S", localtime(&timer));
    return timeBuff;
}

char* getShortTime(char* timeBuff, uint8_t buffLen){
    time_t timer=time(NULL);
    strftime(timeBuff, buffLen, "%H:%M:%S", localtime(&timer));
    return timeBuff;
}

uint8_t vaildMasterMessage(char* data){
    if(strncasecmp(data, command.msg_prefix, strlen(command.msg_prefix)) == 0){
        return 1;
    }else{
        return 0;
    }
}

uint16_t* getGroupDataPos(char* data, uint16_t dataLen, uint32_t* out_groupNum){
    uint32_t idx = 0;
    uint32_t totalNum = 1;
    char *p = data;
    
     for(; (p - data) <= dataLen;){
        if( strncmp(p, command.outer_delim, strlen(command.outer_delim)) == 0 ){
            totalNum ++;
        }
        p += strlen(command.outer_delim);
    }
    uint16_t* pos = (uint16_t*) malloc(sizeof(uint16_t) * totalNum);

    p = data + strlen(command.msg_prefix);    // skip $$
    pos[idx++] = (uint16_t)(p - data);
    strtok(p, command.outer_delim);
    while(( p = strtok(NULL, command.outer_delim)) != NULL){
        pos[idx++] = (uint16_t)(p - data);
    }
    *out_groupNum = idx;
    return pos;
}

uint16_t* getInnerDataPos(char* data, uint32_t* groupNum){
    uint32_t idx = 0;
    uint32_t totalNum = 1;
    char *p = data;
    uint32_t len = strlen(p);

    for(; (p - data) <= len;){
        if( strncmp(p, command.inner_delim, strlen(command.inner_delim)) == 0 ){
            totalNum ++;
        }
        p += strlen(command.inner_delim);
    }
    
    uint16_t * pos = (uint16_t*) malloc(sizeof(uint16_t) * totalNum);
    p = data;
    pos[idx++] = 0;
    strtok(p, command.inner_delim);
    while((p = strtok(NULL,command.inner_delim)) != NULL && (p-data) <= len){
        pos[idx++] = (uint16_t)(p - data);
    }
    *groupNum = idx;
    return pos;
}

/**
 *  format: 
 *      $$3
 *      $$3&ip1:port
 * @param removeIP_port
 * @param data
 */
void requestWorkerIPs(char* removeIP_port, char* out_data, uint16_t * out_dataLen){
    char* p = out_data;
    uint16_t offset = sizeof (uint16_t); //skip length itself
    strcpy(p + offset, command.msg_prefix);
    offset += (uint16_t) strlen(command.msg_prefix);  // $$
    out_data[offset] = (char)reqIps;                                                   // 3
    offset ++;
    
    if (removeIP_port != NULL) {
        strcpy(out_data+offset, command.outer_delim);       // &
        offset += strlen(command.outer_delim);
        
        strcpy(out_data+offset, removeIP_port);
        offset += strlen(removeIP_port);
        printf("Request worker list from master, except %s , str =' %s'\n", removeIP_port, out_data + sizeof(uint16_t));
    } else {
        printf("Request worker list from master. str = '%s' \n", out_data + sizeof(uint16_t));
    }
    *out_dataLen = offset;
}