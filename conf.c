/*************************************************************************
	> File Name: conf.c
	> Author: 
	> Mail: 
	> Created Time: Tue 19 May 2015 11:04:34 PM PDT
 ************************************************************************/
#include "conf.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>		//isblank()
#include <errno.h>
#include <unistd.h>                     // access

//**********************************************************
//      Persional Variabe Definition
//**********************************************************

// filePath[0] ---> netflow.conf   NETFLOW_CONF_HOME
// filePath[1] ---> master
static char filePath[2][100];

//**********************************************************
//      Persional Function Statement
//**********************************************************
static void getfilePath(void);
static BOOLEAN isSkipRow(char* line );
static uint32_t getRealRowsNum( FILE* fp );
static void defaultValue(void);
static void setVariable(char* key, char* value);
static BOOLEAN readConfFile(void);
static BOOLEAN readMasterFile(void);

//**********************************************************
//      Global Function Implement
//**********************************************************
BOOLEAN configure(void){
    getfilePath();
    if(!readConfFile() || !readMasterFile()){
        return FALSE;
    }else{
        return TRUE;
    }
}

//**********************************************************
//      Persional Function Implement
//**********************************************************
static void getfilePath(void){
    
    // get configure directory path
    char* file = getenv("NETFLOW_COLLECTOR_CONF_DIR");
    if( file == NULL ){
        char* homePath = getenv("NETFLOW__COLLECTOR_HOME");
        if(homePath != NULL){
            strncpy(filePath[0], homePath, strlen(homePath));
        }else{
            getcwd(filePath[0], sizeof(filePath[0]));
        }
        strcat(filePath[0],"/conf");
    }else{
            strncpy(filePath[0], file, strlen(file));
    }

    strncpy(filePath[1], filePath[0], strlen(filePath[0]));   
    
    // get netflow.conf  
    strcat(filePath[0],"/netflow.conf");

    // get master file
    strcat(filePath[1],"/master");
}

static BOOLEAN readConfFile(void){

    //load default value
    defaultValue();

    // if file does not exist
    if( !access(filePath[0],F_OK)==0)  {
        return FALSE ;
    }

    // file exist
    FILE* fp =fopen(filePath[0],"r");
    if( fp == NULL ){
        printf("Can not open file %s", filePath[0]);
        //LogWrite(ERROR,"Open %s Fail! %s",strerror(errno));
        return FALSE ;
    }

    // read data
    char line[100]={0}; 
    while(fgets(line, sizeof(line), fp)!= NULL){
        if( isSkipRow(line) )
            continue; 

        char* pos = strchr(line, '=');
        if( pos == NULL ) continue;

        char* key = line;
        char* value = pos+1;
        *pos = '\0';
        value[strlen(value)-1] = '\0';    // remove '\n'

        setVariable(key,value);
        memset(line, 0, 100);
    }
    fclose(fp);
    return TRUE;
    //LogWrite(INFO,"Read %s file finished!",filePath[0]);
}

static BOOLEAN readMasterFile(void){
    if( !access(filePath[1],F_OK)==0)  {
        return FALSE;
    }

    FILE* fp =fopen(filePath[1],"r");
    if( fp == NULL ) {
        return FALSE;
    }
    
    uint32_t rows = getRealRowsNum(fp);
    if( rows == 0 ){
         return FALSE;
    }

    // malloc masterList
    masterList.masterIP = (struct sockaddr_in *)malloc(rows * sizeof(struct sockaddr_in));
    masterList.masterNum = 0;

    // read data
    char line[100]={0}; 
    while(fgets(line, sizeof(line), fp)!= NULL){
        if( isSkipRow(line) ){
            continue;  
        }
     
        char* pos = strchr(line, ':');
        if( pos == NULL ) continue;

        int port = atoi( pos + 1);
        *pos='\0';
        char *ip = line;
        ip = del_both_trim(ip);
        
        setAddress( masterList.masterIP + masterList.masterNum, ip, port );
        masterList.masterNum ++ ;     
        memset(line, 0, 100);
    }
    fclose(fp);
    return TRUE;
}

static BOOLEAN isSkipRow(char* line ){
    char* p = line;
    p = del_left_trim(p);   // skip " "

    // skip ' '、 \t 、 \r 、 \n 、 \v 、 \f 
    if( isspace(p[0]) ) return TRUE;
    if( p[0] == '#') return TRUE;
    return FALSE;
}

static uint32_t getRealRowsNum( FILE* fp ){
    uint32_t rows = 0;
    long filePos = ftell(fp);
    char tmp[200];
    while(fgets( tmp, 200, fp )!= NULL ){
        if( isSkipRow(tmp)) continue;
        rows ++;
        memset( tmp, 0, 200 );
    }
    fseek(fp, filePos, SEEK_SET );
    return rows;
}

static void defaultValue(void){
    if(netflowConf.singleWaitSecond == 0)   
        netflowConf.singleWaitSecond = 40;		
    if(netflowConf.totalMaxTryNum==0)
        netflowConf.totalMaxTryNum = 5;	
    if(netflowConf.receiverWaitSecond==0)
        netflowConf.receiverWaitSecond = 20;
}

static void setVariable(char* key, char* value){
    char* _key = key;
    _key = del_both_trim(_key);
    char* _value = value;
    _value = del_both_trim(_value);

    if(strcasecmp(key,"singleWaitSecond")==0){
        netflowConf.singleWaitSecond = atoi(value);
    }else if(strcasecmp(key,"totalMaxTryNum")==0){
        netflowConf.totalMaxTryNum = atoi(value);
    }else if(strcasecmp(key,"receiverWaitSecond")==0){
        netflowConf.receiverWaitSecond = atoi(value);
    }
    
    // for test
    if(strcasecmp(key,"testLoadData")==0){
        strcpy(netflowtest.testLoadData,value);
    }else if(strcasecmp(key,"testLoadTemp")==0){
        strcpy(netflowtest.testLoadTemp,value);
    }else if(strcasecmp(key,"testLoadMix")==0){
        strcpy(netflowtest.testLoadMix,value);
    }else if(strcasecmp(key,"testLoadv5")==0){
        strcpy(netflowtest.testLoadV5,value);
    }
}

