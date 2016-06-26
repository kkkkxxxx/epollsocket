
#ifndef COMMON_H
#define	COMMON_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <malloc.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/epoll.h>
#include "crc32.h"
#include "listentry.h"

#define EPOLLFLAGS_RECVHEAD    0x00000001
#define EPOLLFLAGS_RECVDATA     0x00000002
#define EPOLLFLAGS_SOCKERROR 0x00000004
#define EPOLLFLAGS_SOCKLISTEN   0x00000008
#define EPOLLFLAGS_SOCKFREE       0x00000010
#define EPOLLFLAGS_CLOSESTATUS  0x00000020

extern int epollfd,epollmaxevents;
extern struct epoll_event * pepollevents;
extern pthread_mutex_t epollsendmutex;
extern pthread_mutex_t epollheartbeatmutex;
extern pthread_mutex_t epollfreemutex;
extern pthread_mutex_t epollrecvqueuemutex;
extern LIST_ENTRY epollheartbeatlisthead;
extern sem_t *pepollrecvqueuesem;
extern LIST_ENTRY epollrecvqueuelisthead;

typedef struct _epollsenddatainfo
{
    LIST_ENTRY sendlist;
    void *epollbuf;
    uint epolllen;
    uint epollseek;
    uint sendlength;
}epollsenddatainfo,*pepollsenddatainfo;

typedef struct _epolldatainfo
{
    LIST_ENTRY heartbeatlist;
    int heartbeatcount;
    int sockfd;
    uint flags;
    uint closestatus;
    uint tag;
    void *recvbuf;
    uint recvlength;
    void *epollbuf;
    uint epolllen;
    uint epollseek;
    LIST_ENTRY sendlisthead;
    void *ipstr;
    void *extptr;
    void (*recvcall)(uint tag,void *buf,uint bufsize,void *extptr,void*ipstr,struct epoll_event *pev);
    void(*disconncall)(void*extptr,void*ipstr);
}epolldatainfo,*pepolldatainfo;


typedef struct _epollrecvqueuedatainfo
{
    LIST_ENTRY recvqueuelist;
    uint tag;
    void *recvbuf;
    uint recvlength;
    void *ipstr;
    void *extptr;
    void (*recvcall)(uint tag,void *buf,uint bufsize,void *extptr,void*ipstr,struct epoll_event *pev);
    struct epoll_event ev;
}epollrecvqueuedatainfo,*pepollrecvqueuedatainfo;

int epollsetnonblocking(int sockfd);

#ifdef	__cplusplus
}
#endif

#endif	/* COMMON_H */

