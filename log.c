
/*************************************************************************
	> File Name: log.c
	> Author: 
	> Mail: 
	> Created Time: Tue 19 May 2015 02:14:55 AM PDT
 ************************************************************************/
#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>       //mkdir
 #include <dirent.h>
#include <sys/types.h> 

typedef struct log{
    char logtime[20];
    char filepath[MAXFILEPATH];
    FILE *fp;
}LOG;
 
typedef struct logseting{
    char filepath[MAXFILEPATH];
    unsigned int maxfilelen;
    unsigned int curfileIdx;
    unsigned char loglevelCode;
}LOGSET;

//**********************************************************
//      Persional Variabe Definition
//**********************************************************
LOGSET logsetting;
LOG loging;
static char confFile[512]={0x0};
const static char LogLevelText[5][8]={"NONE","ERROR","WARN","INFO","DEBUG"};

//**********************************************************
//      Persional Function Statement
//**********************************************************
static int menuToidx( int code );
static unsigned char getLevelcode(char* path );
static void getlogset(void);
static int initlog(unsigned char logLevelCode);
static void defaultValue(LOGSET* logset);
static void setVariable(char* key, char* value, LOGSET* logset);
static char * del_both_trim(char * str) ;
static int isSkipRow(char* line );

 //**********************************************************
//      Global Function Implement
//**********************************************************
 // home/ss/conf/log.conf
void getConfPath(char* path ){
    if(path != NULL){
        strcpy(confFile, path);
    }else{
        getcwd(confFile, sizeof(confFile));    
        strcat(confFile,"/conf/log.properties");
    }
}

int LogWrite(unsigned char loglevel, char *format, ...){
    va_list argList;

    //init log
    if(initlog(loglevel) == 0)  return -1;

    //print log information
    va_start(argList, format);
    vfprintf(loging.fp, format, argList);
    if((logsetting.loglevelCode  & 0x80) != 0){
        vprintf(format, argList);
    }
    va_end(argList);

    if(index(format, '\n') == 0){
        if( (logsetting.loglevelCode  & 0x80) != 0){
            printf("\n");
        }
        fprintf(loging.fp,"\n ");
    }
    
    fflush(loging.fp);
    if(loging.fp!=NULL)  fclose(loging.fp);
    loging.fp=NULL;
    return 0;
}

void logDecollator(){
    LogWrite(INFO,"=================================================");
}

 //**********************************************************
//      Persional Function Implement
//**********************************************************
static int menuToidx( int code ){
    switch( code ){
        case 0: return 0; 
        case 1: return 1; 
        case 2: return 2; 
        case 4: return 3; 
        case 8: return 4; 
        default:
                return 16;
    }
}

static unsigned char getLevelcode(char* value ){
    unsigned char code=255;
    if(strcasecmp("NONE",value)==0)
        code=0;     // 0  -->NONE 0
    else if(strcasecmp ("ERROR",value)==0)
        code=1;     // 1  -->ERROR 1
    else if(strcasecmp ("WARN",value)==0)
        code=3;     // 11   -->WARN 10 || ERROR 1
    else if(strcasecmp ("INFO",value)==0)
        code=7;     // 111  --->INFO 100 || WARN 10 || ERROR 1
    else if(strcasecmp ("DEBUG",value)==0)
        code=15;    // 1110 --->DEBUG 1000 || INFO 100 || WARN 10 || ERROR 1
    else if(strcasecmp ("console",value)==0)
        code= 0x80;
    else
        code=7;
    return code;
} 

static void getlogset(void){
    defaultValue(&logsetting);
    
    if(strlen(confFile) == 0)  {
        printf("Will load default log set.\n");
        return;
    }
    
    FILE *fp = fopen(confFile,"r");
    if(fp==NULL)  {
        printf("Can not open file %s\n", confFile);
        return ; 
    }
        
    char line[100]={0}; 
     while(fgets(line, sizeof(line), fp) != NULL){
        if( isSkipRow(line) )   continue; 

        char* pos = strchr(line, '=');
        if( pos == NULL ) continue;

        char* key = line;
        char* value = pos+1;
        *pos = '\0';
        value[strlen(value)-1] = '\0';    // remove '\n'

        setVariable(key, value, &logsetting);
        memset(line, 0, 100);
    }
}
 
static int initlog(unsigned char logLevelCode){
    char fileName[30]={0x0};

    //init log information
    if( strlen(logsetting.filepath) == 0 ){
        getConfPath(NULL);
        getlogset();
    }
    
    // log level
    if((logLevelCode & logsetting.loglevelCode ) != logLevelCode || (logsetting.loglevelCode & 0x7F) == 0)
        return 0;
 
    memset(&loging, 0, sizeof(LOG));
    
    //get current system time 
    time_t timer=time(NULL);
    strftime(loging.logtime, 20, "%Y-%m-%d %H:%M:%S", localtime(&timer));
    
    // get the log file name 
    strftime(fileName,11,"%Y-%m-%d",localtime(&timer));
    strcat(fileName,".log");

    strncpy(loging.filepath, logsetting.filepath, strlen(logsetting.filepath));
    
    if( opendir(loging.filepath) == NULL ){
        mkdir(loging.filepath, S_IRUSR | S_IWUSR | S_IXUSR);
    }
    
    strcat(loging.filepath, fileName);

    // open log file 
    if(loging.fp == NULL)
        loging.fp=fopen(loging.filepath, "a+");
    
    if(loging.fp == NULL){ 
        perror("Open Log File Fail!");
        return 0;
    }
    
    // write log level and time to file 
    fprintf(loging.fp,"[%s] [%s]: ",LogLevelText[menuToidx(logLevelCode)], loging.logtime);
    return 1;
}
 
static void defaultValue(LOGSET* logset){
    logset->curfileIdx = 0;
    logset->maxfilelen = 10 * 1024 * 1024;
    logset->loglevelCode = getLevelcode("INFO");
    getcwd(logset->filepath, sizeof(logset->filepath));
    strcat(logset->filepath, "/log/");
}

static void setVariable(char* key, char* value, LOGSET* logset){
    char* _key = key;
    _key = del_both_trim(_key);
    char* _value = value;
    _value = del_both_trim(_value);

    if(strcasecmp(key,"filepath")==0){
        strcpy(logset->filepath, value);
        strcat(logset->filepath, "/");
    }else if(strcasecmp(key,"loglevel")==0){
        // INFO or INFO.console
        char* pos;
        if( (pos = index(value, ',')) == 0){
            logset->loglevelCode = getLevelcode(value);
        }else{
            *pos= '\0';
            logset->loglevelCode = getLevelcode(value);
            if(strcasecmp(pos+1,"console")==0){
                logset->loglevelCode |= getLevelcode(pos+1);
            }
        }
    }else if(strcasecmp(key,"maxfilelen")==0){
        logset->maxfilelen = atoi(value);
    } 
}

/*   delete left&&rigth blank space   */
static char * del_both_trim(char * str) {   
    char* pp = str;
     for (;*pp != '\0' && isblank(*pp) ; ++pp);
     
    char *p;
    for (p = pp + strlen(pp) - 1; p >= pp && isblank(*p); --p);
    *(++p) = '\0';
    return pp;
}

static int isSkipRow(char* line ){
    char* p = line;
    for (;*p != '\0' && isblank(*p) ; ++p);

    // skip ' '、 \t 、 \r 、 \n 、 \v 、 \f 
    if( isspace(p[0]) ) return 1;
    if( p[0] == '#') return 1;
    return 0;
}


