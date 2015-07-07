/* 
 * File:   hash.h
 * Author: 
 *
 * update format:
 *     key = {"First Name,Kobe,2;Last Name,Bryant,1;address,USA,4;phone,26300788,5;k101,Value1,3;k110,Value2,3;phone,9433120451,0;First Name,*,0;Last Name,*,2;address,*,4;k101,*,5;k110,*,1;phone,*,5"};
        value= {"0,0;126,1;127,2;192,3;225,4;252,5"};
 * 
 */

#ifndef  HASH_H
#define HASH_H

#define HASHSIZE 100

typedef struct {
    char src_id[20];
    char src_port[20];
    int drs_index;
} router_t;

typedef struct node{
    char drs_ip[20];
    char drs_rate[10];
    char key[30];
    struct node *next;
} node_t;

node_t* search(char* key) ;
void update_hash(char *update_router, char *update_node) ;
void displayHashTable(void);

#endif	/* HASH_H */
