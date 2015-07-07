/*************************************************************************
        > File Name: log.h
        > Author: 
        > Mail: 
        > Created Time: Wed 20 May 2015 08:06:45 AM PDT
 ************************************************************************/

#ifndef LOG_H
#define	LOG_H

#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include <time.h>
#include "stdarg.h"
#include <unistd.h>
 
#define MAXLEN (2048)
#define MAXFILEPATH (512)
#define MAXFILENAME (50)
 
 // for program
typedef enum{ NONE=0, ERROR=1, WARN=2, INFO=4, DEBUG=8 }LOGLEVEL;

void getConfPath( char* path );    // path==NULL -->current dir
int LogWrite(unsigned char loglevel, char *format, ...);
void logDecollator();

#endif	/* LOG_H */

