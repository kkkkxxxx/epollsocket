#include "common.h"
#include "epollworkthread.h"

int epollfd,epollmaxevents=0;
struct epoll_event * pepollevents=NULL;
pthread_mutex_t epollsendmutex;
pthread_mutex_t epollheartbeatmutex;
pthread_mutex_t epollfreemutex;
pthread_mutex_t epollrecvqueuemutex;
LIST_ENTRY epollheartbeatlisthead;
sem_t *pepollrecvqueuesem;
LIST_ENTRY epollrecvqueuelisthead;

int epollsocketsend(uint tag,void*buf,uint bufsize,struct epoll_event *pev)
{
    pepolldatainfo pepolldata;
    pepollsenddatainfo pepollsenddata;
    void *sendbuf;
    do{
        if((pepolldata=pev->data.ptr)==NULL || bufsize==0 || tag==0)break;
        if((pepolldata->flags & EPOLLFLAGS_SOCKERROR) || (pepolldata->closestatus & EPOLLFLAGS_CLOSESTATUS))break;
        if((pepollsenddata=calloc(1,sizeof(epollsenddatainfo)))==NULL)break;
        if((sendbuf=calloc(1,bufsize+12))==NULL){free(pepollsenddata);break;}
        *((uint*)sendbuf)=tag;
        *((uint*)sendbuf+1)=bufsize;
        *((uint*)sendbuf+2)=crc32(0,sendbuf,8);
        memmove((void*)((uint*)sendbuf+3),buf,bufsize);
        pepollsenddata->epollbuf=sendbuf;
        pepollsenddata->epollseek=0;
        pepollsenddata->sendlength=bufsize+12;
        pepollsenddata->epolllen=bufsize+12;
        pthread_mutex_lock(&epollsendmutex);
        if((pepolldata->flags & EPOLLFLAGS_SOCKERROR) || (pepolldata->closestatus & EPOLLFLAGS_CLOSESTATUS))
        {
            free(sendbuf);
            free(pepollsenddata);
            pthread_mutex_unlock(&epollsendmutex);
            return -1;
        }
        else
        {
            InsertTailList(&pepolldata->sendlisthead,&pepollsenddata->sendlist);
            pev->events=EPOLLIN | EPOLLOUT | EPOLLET | EPOLLERR | EPOLLHUP;
            epoll_ctl(epollfd,EPOLL_CTL_MOD,pepolldata->sockfd,pev);
            pthread_mutex_unlock(&epollsendmutex);
            return 0;
        }
    }while(false);
   return -1;
}

void epollsocketclose(struct epoll_event *pev)
{
    pepolldatainfo pepolldata;
    do{
        pepolldata=pev->data.ptr;
        pepolldata->closestatus|=EPOLLFLAGS_CLOSESTATUS;
        pev->events=EPOLLIN | EPOLLOUT | EPOLLET | EPOLLERR | EPOLLHUP;
        epoll_ctl(epollfd,EPOLL_CTL_MOD,pepolldata->sockfd,pev);
    }while(false);
}

int epollsetnonblocking(int sockfd) 
{
    int flags,eax;
    do
    {
        if((flags = fcntl(sockfd,F_GETFL,0))==-1)break;
        flags |= O_NONBLOCK;
        if((eax=fcntl(sockfd,F_SETFL,flags))==-1)break;
        return 0;
    }while(false);
    perror("fcntl:");
    return -1;
}

int epollsocketlisten(ushort port,uint extsize,
        void (*recvcall)(uint tag,void *buf ,uint bufsize,void *extptr,void*ipstr,struct epoll_event *pev),
        void(*disconncall)(void*extptr,void*ipstr))
{
    int lisfd,eax;
    struct sockaddr_in sersa;
    struct epoll_event ev;
    pepolldatainfo pepolldata;
    do{
        if(extsize==0)break;
        memset(&sersa,0,sizeof(struct sockaddr_in));
        sersa.sin_family=AF_INET;
        sersa.sin_addr.s_addr=htonl(INADDR_ANY);
        sersa.sin_port=htons(port);
        if((lisfd=socket(AF_INET,SOCK_STREAM,0))<0)break;
        eax=epollsetnonblocking(lisfd);
        if((eax=bind(lisfd,(struct sockaddr *)&sersa,sizeof(struct sockaddr_in)))<0){close(lisfd);break;}
        if((eax=listen(lisfd,777))<0){close(lisfd);break;}
        pepolldata=calloc(1,sizeof(epolldatainfo));
        pepolldata->sockfd=lisfd;
        pepolldata->flags |=EPOLLFLAGS_SOCKLISTEN;
        pepolldata->epolllen=extsize;
        pepolldata->recvcall=recvcall;
        pepolldata->disconncall=disconncall;
        ev.data.ptr=pepolldata;
        ev.events=EPOLLIN | EPOLLET | EPOLLERR | EPOLLHUP;
        eax=epoll_ctl(epollfd,EPOLL_CTL_ADD,lisfd,&ev);
        return 0;
    }while(false);
    printf("listen:%d:%s\n",errno,strerror(errno));
    return -1;
}

void* epollsocketconnect(char* addrstr,ushort port,uint extsize,struct epoll_event *pev,
         void (*recvcall)(uint tag,void *buf ,uint bufsize,void *extptr,void*ipstr,struct epoll_event *pev),
         void(*disconncall)(void*extptr,void*ipstr))
{
    struct sockaddr_in sa;
    int sockfd,eax;
    pepolldatainfo pepolldata;
    struct hostent *hs;
    char **addrlist;
    do{
        memset(pev,0,sizeof(struct epoll_event));
        if(extsize==0)break;
        if((hs=gethostbyname(addrstr))==NULL)break;
        addrlist=hs->h_addr_list;
        memset(&sa,0,sizeof(struct sockaddr_in));
        sa.sin_family=AF_INET;
        sa.sin_port=htons(port);
        sa.sin_addr.s_addr=*(in_addr_t*)*addrlist;
        if((sockfd=socket(AF_INET,SOCK_STREAM,0))<0)break;
        if((eax=connect(sockfd,(struct sockaddr*)&sa,sizeof (struct sockaddr_in)))<0){close(sockfd);break;}
        eax=epollsetnonblocking(sockfd);
        pepolldata=calloc(1,sizeof(epolldatainfo));
        pepolldata->heartbeatcount=2;
        pepolldata->sockfd=sockfd;
        pepolldata->flags |=EPOLLFLAGS_RECVHEAD;
        pepolldata->epollseek=0;
        pepolldata->epollbuf=calloc(1,12);
        pepolldata->epolllen=12;
        pepolldata->recvbuf=calloc(1,12);
        pepolldata->recvlength=12;
        InitializeListHead(&pepolldata->sendlisthead);
        pepolldata->ipstr=calloc(1,32);
        pepolldata->extptr=calloc(1,extsize);
        pepolldata->recvcall=recvcall;
        pepolldata->disconncall=disconncall;
        pev->data.ptr=pepolldata;
        pev->events=EPOLLIN | EPOLLET | EPOLLERR | EPOLLHUP;
        eax=epoll_ctl(epollfd,EPOLL_CTL_ADD,sockfd,pev);
        pthread_mutex_lock(&epollfreemutex);
        InsertTailList(&epollheartbeatlisthead,&pepolldata->heartbeatlist);
        pthread_mutex_unlock(&epollfreemutex);
        return pepolldata->extptr;
    }while(false);
    printf("connect:%d:%s\n",errno,strerror(errno));
    return NULL;
}

int epollsocketinit(int maxconn)
{
    pthread_t tid;
    pthread_attr_t attr;
    int pid;
    char buf[512];
    do{
        if(pepollevents!=NULL)break;
        //mallopt(M_MMAP_THRESHOLD,0);
        epollmaxevents=maxconn;
        if((epollfd=epoll_create(epollmaxevents))==-1)break;
        if((maxconn=pthread_mutex_init(&epollsendmutex,NULL))!=0){close(epollfd);break;}
        if((maxconn=pthread_mutex_init(&epollheartbeatmutex,NULL))!=0){close(epollfd);break;}
        if((maxconn=pthread_mutex_init(&epollfreemutex,NULL))!=0){close(epollfd);break;}
        if((maxconn=pthread_mutex_init(&epollrecvqueuemutex,NULL))!=0){close(epollfd);break;}
        if((pepollevents=calloc(epollmaxevents,sizeof(struct epoll_event)))==NULL){close(epollfd);break;}
        InitializeListHead(&epollheartbeatlisthead);
        InitializeListHead(&epollrecvqueuelisthead);
        crc32(0,(void*)buf,8);
        memset(buf,0,512);
        pid=getpid();
        sprintf(buf,"/%d%s",pid,"epollrecvqueuesem");
        sem_unlink(buf);
        pepollrecvqueuesem=sem_open(buf,O_CREAT,0644,0);
        if(pepollrecvqueuesem==SEM_FAILED)
        {
            free(pepollevents);
            pepollevents=NULL;
            close(epollfd);
            break;
        }
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
        pthread_create(&tid,&attr,epollworkthread,(void*)0);
        pthread_create(&tid,&attr,epollrecvqueuethread,(void*)0);
        pthread_create(&tid,&attr,epollheartbeatthread,(void*)0);
        pthread_attr_destroy(&attr);
        return 0;
    }while(false);
    printf("init:%d:%s\n",errno,strerror(errno));
    return -1;
}

