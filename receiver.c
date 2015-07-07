/*************************************************************************
        > File Name: receiver.c
        > Author:
        > Mail:
        > Created Time: Thu 14 May 2015 06:44:40 AM PDT
 ************************************************************************/
#include  "receiver.h"

#include "conf.h"
#include "utils.h"
#include "hash.h"
#ifdef LOG
#include "log.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <errno.h>
#include <memory.h>
#include <unistd.h>
#include <signal.h>

#define DEFAULT_BUFFER 1600
#define DEFAULT_WORKER_BASE_NUM 5
#define DEFAULT_MAX_ZERO_COUNT 10

typedef enum {
    connected, connecting, closed, retry, failed, fd_error
} m_status;

//**********************************************************
//      Persional Variabe Definition
//**********************************************************

static struct worker_list_t {
    uint32_t baseIncNum;

    char** workerList;                           // IP string , like "1.2.3.4:10010"
    int* c_clientfd;                                 // the point to client fd ( map to workerList one by one )
    struct sockaddr_in* workerIP;       // ip Address
    m_status* workersStatus;              // worker status
    uint32_t maxNum;                          // the size of workerList we have malloc

    uint32_t activeNum;                       // the number of the workers that is active
    uint32_t* active_to_idx;                  // the idx map to active c_clentfd
} worker_list;

struct masterStatus_t {
    int m_clientfd;
    char recvBuff[1024];
    char sendBuff[1024];
    uint16_t sendLen;
    uint32_t retryMasteridx;
    uint32_t retryTimes;            // retry times
    m_status status;

    enum { off, on } report;
} masterStatus;

static buffer_s sendbuffer;

static fd_set readSet;
static fd_set writeSet;
struct timeval select_tm;

static uint32_t worker_idx = 0;

//**********************************************************
//      Persional Function Statement
//**********************************************************
static BOOLEAN initSendBuff(void);
static BOOLEAN initMasterClient(void);
static BOOLEAN initWorkerList(void);
static BOOLEAN setTcpKeepAlive(int fd, int start, int interval, int count);

static m_status tryConnectMaster(void);
static m_status trySingleMasterConnect(struct sockaddr_in* ip);
static void retryConnectMaster(void);

static int resetFdset(struct buffer_s* data);
static int resetMasterFdSet(int curfd);
static int resetWorkerFdset(int curfd, struct buffer_s* data);

static void dealWithMasterSet(void);
static BOOLEAN dealWithWorkerSet(struct buffer_s* data);
static void dealWithMasterData(char* masterData, uint32_t dataLen);

static void updateRule(char* key, char* value);
static void updateWorkerList(char* workerList);

static BOOLEAN resetWorkListSpace(uint32_t needNum);
static int createSocket(void);
static void requestMaster(char* removeIP_port);

static void addNewConnect(char* ip, int index);
static void deleExistConnect(int index);
static void updateActiveClientfd(void);

//**********************************************************
//      Global Function Implement
//**********************************************************

BOOLEAN initClient(void) {
    printf("Start to run receiver component....\n");
    if(!initSendBuff()){
        printf("init send buffer error");
        return FALSE;
    }
    if(!initMasterClient() ){
        printf("init masrer client error!");
        return FALSE;
    }
    if(!initWorkerList() ){
        printf("init worker client error!");
        return FALSE;
    }

    setTcpKeepAlive(masterStatus.m_clientfd, 10, 10, 5);
 //   signal(SIGPIPE, SIG_IGN);
    requestMaster(NULL);
    return TRUE;
}

BOOLEAN runClient(struct buffer_s* data) {
    BOOLEAN result = FALSE;
    int maxfd = resetFdset(data);
    int no = select(maxfd, &readSet, &writeSet, NULL, &select_tm);
    switch (no) {
        case -1:
            printf("The receiver select error!! %s ...\n", strerror(errno));
            break;
        case 0:
            if (masterStatus.status == retry || masterStatus.status == connecting) {
                retryConnectMaster();
            }
            break;
        default:
            dealWithMasterSet();
            result = dealWithWorkerSet(data);
    }
    return result;
}

#ifdef TEST

struct buffer_s* fillNetflowData(testData* eth_hdr) {
    // TODO here we will get the netflow data
    if (!eth_hdr)   return NULL;

    //  data format :
    // totalLen(1int) + ipflage(1byte) + ipAddress(4|16Byte) + netflowData(nByte)
    uint16_t payload_len = (uint16_t) eth_hdr->length;
    uint8_t ipLen = 4;
    // totalLen + char + ipLen + payLoad
    uint16_t totalLen = sizeof (uint16_t) + sizeof (uint8_t) +ipLen + payload_len;

    if (sendbuffer.buffMaxLen < totalLen) {
        free(sendbuffer.buff);
        sendbuffer.buff = (u_char*) malloc(totalLen);
        if (sendbuffer.buff == NULL) {
            return NULL;
        }
        sendbuffer.buffMaxLen = totalLen;
    }

    u_char* p = sendbuffer.buff;
    memcpy(p, &totalLen, sizeof (uint16_t));
    p = p + sizeof (uint16_t);

    *p = (uint8_t) ipLen;
    p++;

    u_char ip[4] = {192, 168, 1, 1};
    memcpy(p, &ip, ipLen); // src IP
    p = p + ipLen;

    memcpy(p, eth_hdr->data, payload_len);
    sendbuffer.bufflen = totalLen;
    return &sendbuffer;
}
#else

struct buffer_s* fillNetflowData(struct ether_hdr* eth_hdr) {
    // TODO here we will get the netflow data
    if (!eth_hdr)  return NULL;

    struct ipv4_hdr *ip_hdr = (struct ipv4_hdr *) (eth_hdr + 1);
    uint8_t ip_len = (ip_hdr->version_ihl & 0xf) * 4;
    //uint32_t src_ip = rte_be_to_cpu_32(ip_hdr->src_addr);
    uint32_t src_ip = ip_hdr->src_addr;
    struct udp_hdr* u_hdr = (struct udp_hdr *) ((u_char *) ip_hdr + ip_len);
    uint16_t payload_shift = sizeof (struct udp_hdr);
    uint16_t payload_len = u_hdr->dgram_len - sizeof(struct udp_hdr);
    u_char *the5Record = (u_char *) u_hdr + payload_shift;

    //  datalen(1int) + ipflage(1byte) + ipAddress(4|16Byte) + netflowData(nByte)
    uint8_t ip_type = (ip_hdr->version_ihl >> 4) == 4 ? 4 : 16;
    uint16_t totalLen = sizeof (uint16_t) + sizeof (uint8_t) + ip_type + payload_len;

    if (sendbuffer.buffMaxLen < totalLen) {
        free(sendbuffer.buff);
        sendbuffer.buff = (u_char*) malloc(totalLen);
        if (sendbuffer.buff == NULL) {
            return NULL;
        }
        sendbuffer.buffMaxLen = totalLen;
    }

    u_char* p = sendbuffer.buff;
   memcpy(p, &totalLen, sizeof (uint16_t));
    p = p + sizeof (uint16_t);

    *p = (uint8_t) ip_type;
    p++;

    memcpy(p, (void *) &src_ip, ip_type); // src IP
    p = p + ip_type;

    memcpy(p, the5Record, payload_len);
    sendbuffer.bufflen = totalLen;
    return &sendbuffer;
}

#endif

//**********************************************************
//      Persional Function Implement
//**********************************************************

static BOOLEAN initSendBuff(void) {
    sendbuffer.buff = (u_char*) malloc(sizeof (char) * DEFAULT_BUFFER);
    if(sendbuffer.buff == NULL)     return FALSE;
    sendbuffer.buffMaxLen = DEFAULT_BUFFER;
    sendbuffer.bufflen = 0;
    return TRUE;
}

static BOOLEAN initMasterClient(void) {
    printf("Try to connect with master client.\n");
    if (tryConnectMaster() != connected) {
        printf("There is no master to connect, the receiver will quit\n");
#ifdef LOG
        LogWrite(INFO,"There is no master to connect, the receiver will quit\n");
#endif
        return FALSE;
    } else {
#ifdef LOG
        LogWrite(INFO,"Master connected\n");
#endif
        return TRUE;
    }
}

 static BOOLEAN initWorkerList(void) {

    worker_list.baseIncNum = DEFAULT_WORKER_BASE_NUM;
    int defaultNum = worker_list.baseIncNum;
    worker_list.workerList = (char**) malloc(sizeof (char*) * defaultNum);
    worker_list.c_clientfd = (int*) malloc(sizeof (int)* defaultNum);
    worker_list.workerIP = (struct sockaddr_in*) malloc(sizeof (struct sockaddr_in)* defaultNum);
    worker_list.workersStatus = (m_status*) malloc(sizeof (m_status) * defaultNum);
    if(worker_list.workerList == NULL ||
            worker_list.c_clientfd == NULL ||
            worker_list.workerIP == NULL ||
            worker_list.workersStatus == NULL){
        return FALSE;
    }

    memset(worker_list.workerList, 0, sizeof (char*)*defaultNum);
    worker_list.maxNum = defaultNum;
    worker_list.activeNum = 0;
    return TRUE;
}

 /**
 *  set the tcp keep alive
 * @param fd
 * @return
 */
static BOOLEAN setTcpKeepAlive(int fd, int start, int interval, int count) {
    int keepAlive = 1;
    if (fd < 0 || start < 0 || interval < 0 || count < 0) {
        return FALSE;
    }

    // set keep alive
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void*) &keepAlive, sizeof (keepAlive)) == -1) {
        perror("setsockopt");
        return FALSE;
    }

    // set start time
    if (setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, (void *) &start, sizeof (start)) == -1) {
        perror("setsockopt");
        return FALSE;
    }
    //set interval between two package
    if (setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, (void *) &interval, sizeof (interval)) == -1) {
        perror("setsockopt");
        return FALSE;
    }
    //set max retry times
    if (setsockopt(fd, SOL_TCP, TCP_KEEPCNT, (void *) &count, sizeof (count)) == -1) {
        perror("setsockopt");
        return FALSE;
    }
    return TRUE;
}

 // ---------------------------------------------------------------------------------------------------------
 // connect with master

 /**
  * Try connect with whole master
  * @return m-status
  */
static m_status tryConnectMaster(void) {
    uint16_t maxRetryNum = netflowConf.totalMaxTryNum;
    printf("Max retry number is %u times.", maxRetryNum);
    printf("Registed %u masters.", masterList.masterNum);
#ifdef LOG
    LogWrite(INFO, "Max retry number is %d times.", maxRetryNum);
    LogWrite(INFO, "Registed %d masters.", masterList.masterNum);
#endif

    uint16_t tryidx = 0;
    while (tryidx != maxRetryNum) {
        printf("\nRetry %u times to connect with whole master!\n", tryidx + 1);
#ifdef LOG
         LogWrite(INFO,"Retry %d times to connect with whole master!", tryidx);
#endif

        // connect with all master
        uint16_t masteridx = 0;
        while (masteridx != masterList.masterNum) {
            struct sockaddr_in* ip = masterList.masterIP + masteridx;
            printf("Try to connect with %s:%d\n", inet_ntoa(ip->sin_addr), ntohs(ip->sin_port));
#ifdef LOG
            LogWrite(INFO,"try to connect with %s:%d", inet_ntoa(ip->sin_addr), ntohs(ip->sin_port));
#endif

            m_status st = trySingleMasterConnect(ip);
            if (st == connected) {
                masterStatus.status = connected;
                return connected;
            } else {
                masteridx++;
            }
        }
        tryidx++;
    }
    masterStatus.status = failed;
    return failed;      // no Master to connect
}

/**
 * Try connect with a single master
 * @param ip
 * @return m_status { failed, connected, fd_error}
 */
static m_status trySingleMasterConnect(struct sockaddr_in* ip) {

    masterStatus.m_clientfd = createSocket();
    if (masterStatus.m_clientfd == -1) {
#ifdef LOG
        LogWrite(ERROR, "Init master client's socket error, %s", strerror(errno));
#endif
        printf("Init master client's socket error, %s\n", strerror(errno));
        return fd_error;
    }

    int res = connect(masterStatus.m_clientfd, (struct sockaddr*) ip, sizeof (struct sockaddr));

    if (res == 0) {
        printf("Connected, Master is %s\n", inet_ntoa(ip->sin_addr));
#ifdef LOG
        LogWrite(INFO, "Connected, Master is %s", inet_ntoa(ip->sin_addr));
#endif
        return connected;
    } else {
        if (errno != EINPROGRESS) {
            printf("Connect failed. %s\n", strerror(errno));
#ifdef LOG
            LogWrite(INFO, "Connect failed. %s", strerror(errno));
#endif
            close(masterStatus.m_clientfd);
            return failed;
        }
    }

    struct timeval tv;
    tv.tv_sec = netflowConf.singleWaitSecond;
    tv.tv_usec = 0;

    errno = 0;
    FD_ZERO(&readSet);
    FD_ZERO(&writeSet);
    FD_SET(masterStatus.m_clientfd, &readSet);
    writeSet = readSet;

    res = select(masterStatus.m_clientfd + 1, &readSet, &writeSet, NULL, &tv);
    switch (res) {
            printf("Select error in connect ... %s\n", strerror(errno));
#ifdef LOG
            LogWrite(ERROR, "Select error in connect ... %s",strerror(errno));
#endif
            close(masterStatus.m_clientfd);
            return failed;

        case 0:
            printf("Select time out.\n");
#ifdef LOG
            LogWrite(INFO, "Select time out.");
#endif
            close(masterStatus.m_clientfd);
            return failed;

        default:
            if (FD_ISSET(masterStatus.m_clientfd, &readSet)
                    || FD_ISSET(masterStatus.m_clientfd, &writeSet)) {
                int no = connect(masterStatus.m_clientfd, (struct sockaddr*) ip, sizeof (struct sockaddr));
                if (no == 0) {
#ifdef LOG
                LogWrite(INFO, "Connected, Master is %s", inet_ntoa(ip->sin_addr));
#endif
                    printf("Connected, Master is %s\n", inet_ntoa(ip->sin_addr));
                    return connected;
                } else {
                    int err = errno;
                    if (err == EISCONN) {
                        //LogWrite(INFO, "Connected, Master is %s", inet_ntoa(ip->sin_addr));
                        printf("Connected, Master is %s\n", inet_ntoa(ip->sin_addr));
                        return connected;
                    } else {
                        //LogWrite(INFO, "Connect failed! %s %d", strerror(err), err);
                       printf("Connect with master %s failed\n", inet_ntoa(ip->sin_addr));
                        close(masterStatus.m_clientfd);
                        return failed;
                    }
                }
            }else{
                printf("unknow error, masterStatus.m_clientfd = %d", masterStatus.m_clientfd);
                return failed;
            }
    }
}

/* disconnected and retry  */


/**
 * Retry connect with master
 * @return m_status { failed, connected, fd_error}
 */
static void retryConnectMaster(void) {
    if (masterStatus.status == retry) {
        masterStatus.m_clientfd = createSocket();
        if (masterStatus.m_clientfd == -1) {
            printf("Init master client's socket error, %s\n", strerror(errno));
            masterStatus.status = fd_error;
        } else {
            masterStatus.status = connecting;
        }
    }

    struct sockaddr_in* ip = masterList.masterIP + masterStatus.retryMasteridx;
    int res = connect(masterStatus.m_clientfd, (struct sockaddr *) ip, sizeof (struct sockaddr));
    if (res == 0) {
        printf("Connected immediately. Master is %s, res=0\n", inet_ntoa(ip->sin_addr));
        masterStatus.status = connected;
        requestMaster(NULL);
        return;
    }

    switch (errno) {
        case EISCONN:
            printf("Connected. Master is %s\n", inet_ntoa(ip->sin_addr));
            masterStatus.status = connected;
            requestMaster(NULL);
            break;
        case EINPROGRESS:
            printf("Connecting %s ...error %s\n", inet_ntoa(ip->sin_addr), strerror(errno));
            masterStatus.status = connecting;
            break;
        case EALREADY:
            printf("Connecting %s, errno = %s, %d\n ", inet_ntoa(ip->sin_addr), strerror(errno), errno);
            masterStatus.status = connecting;
            break;
        default:
            close(masterStatus.m_clientfd); // close current fd.
            masterStatus.retryMasteridx++;

            if (masterStatus.retryMasteridx == masterList.masterNum) {
                printf("Connect %s failed. errno = %d . %s. \n",
                        inet_ntoa(ip->sin_addr), errno, strerror(errno));
                masterStatus.retryMasteridx = 0;
                masterStatus.retryTimes++;

                if (masterStatus.retryTimes == netflowConf.totalMaxTryNum) {
                    printf("There is no aliving LoadMaster to connect with.\n");
                    masterStatus.status = failed;
                } else {
                    printf("Try %dth times to connect LoadMaster.\n", masterStatus.retryTimes);
                    masterStatus.status = retry;
                }
            } else {
                printf("Connect %s failed. Errno = %d. %s, change another LoadMaster.\n ",
                        inet_ntoa(ip->sin_addr), errno, strerror(errno));
                masterStatus.status = retry;
            }
    }
}


//-------------------------------deal with run function -------------------------------------------------

static int resetFdset(struct buffer_s* data) {
    int fd = 0;
    FD_ZERO(&readSet);
    FD_ZERO(&writeSet);
    fd = resetMasterFdSet(fd);
    fd = resetWorkerFdset(fd, data);

    // init select_time
    select_tm.tv_sec = netflowConf.receiverWaitSecond;
    return fd + 1;
}

static int resetMasterFdSet(int curfd) {
    // Focus on readSet, to get the control information from master.
    // If we need to send the data to master, we should register in writeSet

    if (masterStatus.status == connected
            || masterStatus.status == retry
            || masterStatus.status == connecting) {
#ifdef LOG
        LogWrite(DEBUG,"FD_SET-master_fd(%d)-readSet",masterStatus.m_clientfd);
#endif
        FD_SET(masterStatus.m_clientfd, &readSet);

        if (masterStatus.report == on
                || masterStatus.status == retry
                || masterStatus.status == connecting) {
#ifdef LOG
        LogWrite(DEBUG,"FD_SET-master_fd(%d)-writeSet",masterStatus.m_clientfd);
#endif       
          FD_SET(masterStatus.m_clientfd, &writeSet);
        }
        return ( curfd > masterStatus.m_clientfd) ? curfd : masterStatus.m_clientfd;
    }
    return curfd;
}

static int resetWorkerFdset(int curfd, struct buffer_s* data) {
    // Focus on readset & writeset.
    // ReadSet only for get the disconnect information, and WriteSet for write data.
    int maxfd = curfd;
    uint32_t i;
    for (i = 0; i != worker_list.activeNum; i++) {
        int idx = worker_list.active_to_idx[i];
        int fd = worker_list.c_clientfd[idx];
        FD_SET(fd, &readSet);
        if (data != NULL)  FD_SET(fd, &writeSet);
        maxfd = maxfd > fd ? maxfd : fd;
    }
    return maxfd;
}

static void dealWithMasterSet(void) {

    if (FD_ISSET(masterStatus.m_clientfd, &readSet)) {

        switch (masterStatus.status) {
            case connected:
                memset(masterStatus.recvBuff, 0, sizeof (masterStatus.recvBuff));

                uint16_t totalLen;
                int readCount = recv(masterStatus.m_clientfd, &totalLen, sizeof(uint16_t), 0);
                printf("totalLen = %d\n", totalLen);
                totalLen = ntohs(totalLen) - 2;
                printf("read total size %d,  read count %d\n", totalLen, readCount);
                
                int curNo = recv(masterStatus.m_clientfd, masterStatus.recvBuff, totalLen, 0 ) ;
                while( curNo != totalLen){
                     if (curNo == -1 || curNo == 0) { // failed connection
                        printf("Disconnect with master, and will retry to connect with master.\n");
                        close(masterStatus.m_clientfd);
                        masterStatus.status = retry;

                        masterStatus.retryTimes = 0;
                        masterStatus.retryMasteridx = 0;
                        retryConnectMaster();
                        break;
                    }else {
                        curNo += recv(masterStatus.m_clientfd, masterStatus.recvBuff + curNo, totalLen - curNo, 0 ) ;
                    }
                }
             //   masterStatus.recvBuff[totalLen]='\0';
                printf("Rece message from master, %s\n",masterStatus.recvBuff);
               dealWithMasterData(masterStatus.recvBuff, totalLen);
                break;

            case retry:
            case connecting:
                retryConnectMaster();
                break;
            default:
                printf("There is no available master to connect with.\n");
//                
//            case failed:
//                printf("current master connected failed.\n");
//                return;
//            case closed:
//                printf("current master has closed! There is no available master to connect with.\n");
//                return;
//            case fd_error:
//                printf("current master's fd is error!\n ");
//                return;
        }
    }

    // here we will deal with sending report and request to master
    if (FD_ISSET(masterStatus.m_clientfd, &writeSet)) {

        switch (masterStatus.status) {
            case retry:
            case connecting:
                retryConnectMaster();
                break;
            case connected:{
                int countNum =
                        send(masterStatus.m_clientfd, masterStatus.sendBuff, masterStatus.sendLen, 0);
                char* bp = masterStatus.sendBuff;
                while (countNum != masterStatus.sendLen) {
                    if (countNum == -1) {
                        printf("Connect master error. %s\n", strerror(errno));
                        close(masterStatus.m_clientfd);
                        masterStatus.status = retry;
                        retryConnectMaster();
                    } else {
                        bp = bp + countNum;
                        countNum = send(masterStatus.m_clientfd, bp, masterStatus.sendLen - countNum, 0);
                    }
                }
                masterStatus.report = off;
            }
                break;
            case failed:
                printf("current master connected failed.\n");
                break;
            case closed:
                printf("error! current master has closed!\n ");
                break;
            case fd_error:
                printf("current master's fd is error! \n");
                break;
        }
    }
}

static BOOLEAN dealWithWorkerSet(struct buffer_s* data) {

    if(worker_list.activeNum == 0 ) return FALSE;
    worker_idx = worker_idx % worker_list.activeNum;

    uint32_t idx = worker_list.active_to_idx[worker_idx];
    int fd = worker_list.c_clientfd[idx];

    /* remove the disconnect socket */
    if (FD_ISSET(fd, &readSet)) {
        int readCount =
        recv(fd, masterStatus.recvBuff, sizeof (masterStatus.recvBuff), MSG_WAITALL | MSG_DONTWAIT);

        if (readCount == -1 || readCount == 0) {
            printf("Disconnect with worker %s\n", worker_list.workerList[idx]);
            FD_CLR(fd, &writeSet);

            // If there is only one worker connected with this receiver, we must request a new worker.
            // If there is more than one workers, ignore the request.
            if (worker_list.activeNum == 1) {
                requestMaster(worker_list.workerList[idx]);
                printf("There is no available worker to send netflow data to.  "
                        "So request worker list to master.\n");
            }

            printf("[dealWithWorkerSet] delete unReachable worker %s\n ", worker_list.workerList[idx]);
            deleExistConnect(idx);
            updateActiveClientfd();
            worker_idx++;
            return FALSE;
        }
    }

    /* write the netflow package to worker*/
    if (FD_ISSET(fd, &writeSet)) {
        int no = send(fd, data->buff, data->bufflen, 0);

        while (no != (int)data->bufflen) {
            if (no == -1) {
                printf("Send ip %s data error.\n", inet_ntoa(worker_list.workerIP[idx].sin_addr));
                break;
            } else {
                no += send(fd, data->buff + no, (size_t) (data->bufflen - no), 0);
            }
        }
        worker_idx++;
        return TRUE;
    }
    return FALSE;
}

/* analysis the data from master, and deal with the data*/
/**
 *
 *workerlistMsg  mode :
 *      $$1&+2;ip1:port;ip2:port     --> add new ip's address
 *      $$1&-1;ip1:port                      --> dele exist ip's address
 *      $$1&+2;ip1:port;ip2:port&-3;1p3:port;ip4:port  -->add and delete
 *
 * ruleMsg mode:
 *      $$2&ip1:1;ip2:1;ip3:2&ipA,ipB
 * @param masterData
 * @param len
 */
static void dealWithMasterData(char* masterData, uint32_t dataLen) {

    uint32_t totalNum = 0;
    char* data = masterData;
    if(vaildMasterMessage(data) == 0){
        printf("Master message error. %s\n", masterData);
        return;
    }

    uint16_t* pos = getGroupDataPos(data, dataLen, &totalNum);
    uint16_t* ppos = pos;
    if(pos == NULL) return;

    int type = atoi(data+(*ppos++));
    uint32_t curNum = 1;    //  skip type
    switch(type){
        case workerlistMsg:
            while(curNum < totalNum){
                updateWorkerList(data + (*ppos++));
                curNum ++;
            }
            break;
        case ruleMsg:
            updateRule(data+pos[1], data+pos[2]);
            break;
        default:
            printf("Unknow message type.\n");
    }
    free(pos);
}

static void updateRule(char* key, char* value){
    update_hash(key,value);
    printf("%s-->%s\n",key, value);
}

/**
 *  Update the  worker_list
 *    mode :
 *      +2;ip1:port;ip2:port     --> add new ip's address
 *      -1;ip1:port                      --> dele exist ip's address
 * @param workerList  the data from master
 */
static void updateWorkerList(char* workerList) {

    uint32_t totalNum = 0;
    uint16_t* pos = getInnerDataPos(workerList, &totalNum);
    uint16_t* p = pos;

    char mode = *(workerList + (*p));
    int num = atoi(workerList + (*p++) + 1);
    if( num == 0) return;

    uint32_t curNum = 1;     // skip mode
    switch(mode){
        case '+':{
             /* update the maxNum */
            if ( resetWorkListSpace(num) == FALSE){
                return;
            }

            while(curNum < totalNum){
                char* ip_port = workerList + (*p++);

                uint32_t i;
                //check if exist in current workerlist
                if (worker_list.active_to_idx != NULL) {
                    for (i = 0; i < worker_list.activeNum; i++) {
                        int idx = worker_list.active_to_idx[i];
                        if (strcasecmp(ip_port, worker_list.workerList[idx]) == 0) {
                            break;
                        }
                    }
                    // exist in current worker_list
                    if (i != worker_list.activeNum) {
                        continue;
                    }
                }

                // add new ip into worker_list
                for (i = 0; i < worker_list.maxNum; i++) {
                    if (worker_list.workerList[i] == NULL) {    // find place to insert into
                        addNewConnect(ip_port, i);
                        printf("[updateWorkerList] Add a new worker %s, current total active num is %d\n",
                                ip_port, worker_list.activeNum);
                        break;
                    }
                }
                curNum++;
            }
            updateActiveClientfd();
        }
        break;

        case '-':{
            uint32_t maxNum =  worker_list.activeNum;
            while(curNum < totalNum){
                char* ip_port = workerList + (*p++);
                uint32_t  i = 0;
                for (; i < maxNum; i++) {
                    int idx = worker_list.active_to_idx[i];
                    if(worker_list.workerList[idx] == NULL) continue;
                    if (strncasecmp(ip_port, worker_list.workerList[idx], strlen(ip_port)) == 0) {
                        deleExistConnect(idx);
                          printf("[updateWorkerList] delete  ip %s, current active worker num %d\n",
                                ip_port, worker_list.activeNum);
                        break;
                    }
                }
                curNum ++;
            }
            updateActiveClientfd();

            // check the current worker list
            if(worker_list.activeNum == 0){
                requestMaster(NULL);
            }
        }
        break;

        default:
            printf("Unknow format\n");
    }
    free(pos);
}

static BOOLEAN resetWorkListSpace(uint32_t needNum) {
    if (needNum > worker_list.maxNum - worker_list.activeNum) {
        uint32_t realNeedNum = needNum - (worker_list.maxNum - worker_list.activeNum);
        uint32_t adjustSize = worker_list.baseIncNum > realNeedNum ? worker_list.baseIncNum : realNeedNum;
        uint32_t newSize = worker_list.maxNum + adjustSize;

        worker_list.workerList = (char**) realloc(worker_list.workerList, newSize);
        worker_list.c_clientfd = (int*) realloc(worker_list.c_clientfd, newSize);
        worker_list.workerIP = (struct sockaddr_in*) realloc(worker_list.workerIP, newSize);
        if (worker_list.workerList == NULL
                || worker_list.c_clientfd == NULL
                || worker_list.workerIP == NULL) {
            return FALSE;
        }
        memset(worker_list.workerList + worker_list.maxNum, 0, adjustSize);
        worker_list.maxNum = newSize;
    }
    return TRUE;
}

// ----------------------- Common function --------------------------------------------------

static int createSocket(void) {
    int tcpfd = socket(AF_INET, SOCK_STREAM, 0);
    if(tcpfd < 0)   return -1;

    int flags = fcntl(tcpfd, F_GETFL, 0);
    if(flags < 0)   return -1;

    int fcn = fcntl(tcpfd, F_SETFL, flags | O_NONBLOCK);
    if(fcn < 0) return -1;

     return tcpfd;
}

/**
 *  The function for request the worker list. command may be like as follow  :
 *              1) $req$                                        --> request the worker list
 *              2) $req$-192.168.80.1:100020     -->request the worker list except 192.168.80.1:100020
 * @param removeIP_port
 *          except removeIP_port, when request the worker list.  The value can be set NULL
 */
static void requestMaster(char* removeIP_port) {

    requestWorkerIPs(removeIP_port, masterStatus.sendBuff, &masterStatus.sendLen);

    // copy the length
    memcpy(masterStatus.sendBuff, &masterStatus.sendLen, sizeof (uint16_t));
    masterStatus.report = on;
    printf("request master for a worker address\n");
}

/**
 * Add new connectting with worker
 * @param ip_port
 * @param index :  point to worker_list.workerList
 */
static void addNewConnect(char* ip_port, int index) {
    worker_list.workerList[index] = strdup(ip_port);
    char* d = strchr(ip_port, ':');
    *d = '\0';
    uint16_t port = atoi(d + 1);

    setAddress(worker_list.workerIP + index, ip_port, port);

    worker_list.c_clientfd[index] = createSocket();
    if (worker_list.c_clientfd[index] == -1) {
        return;
    }
    connect(worker_list.c_clientfd[index], (struct sockaddr *) (worker_list.workerIP + index),
            sizeof (struct sockaddr));
    printf("Add the workerIP, fd=%d ip=%s:%d\n", index, ip_port, port);
    worker_list.activeNum++;
    worker_list.workersStatus[index] = connecting;
}

/**
 * Delete exist connectting.
 * @param index point to worker_list.workerList
 */
static void deleExistConnect(int index) {
    free(worker_list.workerList [index]); //free the char* point
    worker_list.workerList[index] = NULL;
    shutdown(worker_list.c_clientfd[index], SHUT_RDWR); //close socket
    close(worker_list.c_clientfd[index]);
    worker_list.activeNum--;
    worker_list.workersStatus[index] = closed;
}

static void updateActiveClientfd(void) {
    //TODO: here should be more effective
    free(worker_list.active_to_idx);
    worker_list.active_to_idx = (uint32_t*) malloc(sizeof (uint32_t) * worker_list.activeNum);

    uint32_t active_idx = 0;
    uint32_t i = 0;
    while (i != worker_list.maxNum) {
        if (worker_list.workerList[i] != NULL) {
            // exist ip
            worker_list.active_to_idx[active_idx++] = i;
            if (active_idx > worker_list.activeNum) {
                return;
            }
        }
        i++;
    }
}