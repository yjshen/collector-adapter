/* 
 * File:   hash.c
 * Author: ayscb
 *
 *  Created Time: Tue 19 May 2015 11:04:34 PM PDT
 */
#include "hash.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>   

//**********************************************************
//      Persional Variabe Definition
//**********************************************************
static node_t* hashtable[HASHSIZE];
static pthread_mutex_t mut;

//**********************************************************
//      Persional Function Statement
//**********************************************************
static int insert(node_t nodeData, router_t routerRecord);       // build hash table
static void cleanUp(void);
static uint32_t hash(char *s);
static node_t* lookup(char *str);
static char* strCat(char* str1, char* str2, char* out_buff, uint32_t buff_len);

//**********************************************************
//      Global Function Implement
//**********************************************************
node_t* search(char* key) {
    node_t* np = lookup(key);
    if (np == NULL)
        return NULL;
    else
        return np;
}

void update_hash(char *update_router, char *update_node) {
    // clock
    pthread_mutex_init(&mut, NULL);
    
    router_t router[100];
    node_t node[100];
    char delims[] = ";,";

    uint32_t i = 0;
    char* element_r = strtok(update_router, delims); 
    while (element_r != NULL) {
        sprintf(router[i].src_id, "%s", element_r);
        element_r = strtok(NULL, delims);
        sprintf(router[i].src_port, "%s", element_r);
        element_r = strtok(NULL, delims);
        router[i].drs_index = atoi(element_r);
        element_r = strtok(NULL, delims);
        i++;
    }

    uint32_t count = i;
    i = 0;
    char* element_n = strtok(update_node, delims); 
    while (element_n != NULL) {
        strcpy(node[i].drs_ip, element_n);
        element_n = strtok(NULL, delims);
        strcpy(node[i].drs_rate, element_n);
        element_n = strtok(NULL, delims);
        i++;
    }
    
    i = 0;
    pthread_mutex_lock(&mut);       //lock
    cleanUp();                                  //cleanUp the old hashtable
    for (i = 0; i < count; ++i) {
        router_t _router = router[i];
        node_t _node= node[_router.drs_index];
        insert(_node, _router);         //build new hashtable
    }
    pthread_mutex_unlock(&mut);         //unlock 
}

void displayHashTable(void){
    node_t *np;
    uint32_t i;
    printf("------------ display result -------------");
    for(i=0; i < HASHSIZE; ++i){
        if(hashtable[i] != NULL) {
            np = hashtable[i];
            printf("hash code: %d (", i);
            for(; np != NULL; np=np->next)
                printf(" (%s.%s.%d ) ", np->key, np->drs_ip, atoi(np->drs_rate));
            printf(")\n");
        }
    }
    printf("------------ display result -------------");
}

//**********************************************************
//      Persional Function Implement
//**********************************************************

/**
 * Insert data into Hash Table to build hash table
 * @param nodeData  
 * @param routerRecord
 * @return 
 */
static int insert(node_t nodeData, router_t routerRecord) {

    char stringCat[20] = {0};
    strCat(routerRecord.src_id, routerRecord.src_port, stringCat, sizeof(stringCat));
    uint32_t hashCode = hash(stringCat);
    
    node_t* newNode = (node_t*) malloc(sizeof (node_t));
    if(newNode == NULL) return 0;
    
    strcpy(newNode->key, stringCat);
    strcpy(newNode->drs_ip, nodeData.drs_ip);
    strcpy(newNode->drs_rate, nodeData.drs_rate);
    newNode->next = hashtable[hashCode];
    hashtable[hashCode] = newNode; 
    return 1;
}

/**
 * Clean all table
 * @return 
 */
static void cleanUp(void) {
    node_t *np, *nq;
    uint32_t i;
    for (i = 0; i < HASHSIZE; ++i) {
        if (hashtable[i] != NULL) {
            np = hashtable[i];
            while (np != NULL) {
                nq = np->next;
                free(np);
                np = nq;
            }
            hashtable[i] = NULL;
        }
    }
}

static uint32_t hash(char *s) {    
   uint32_t code = 0;
    for (; *s; s++){
        code = *s + code * 31; 
    }        
    return code % HASHSIZE;
}

static node_t* lookup(char *str) {
    uint32_t hashCode = hash(str);
    node_t* np = hashtable[hashCode];
    for (; np != NULL; np = np->next) {
        if (!strcmp(np->key, str))  return np; 
    }
    return NULL;
}

static char* strCat(char* str1, char* str2, char* out_buff, uint32_t buff_len){
    if(strlen(str1) + strlen(str2) > buff_len){
        return NULL;
    }
    strcpy(out_buff, str1);
    strcat(out_buff, str2);
    return out_buff;
}