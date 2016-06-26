#include "common.h"
#include "epollworkthread.h"

void epollworkfree(struct epoll_event *pev)
{
    pepolldatainfo pepolldata;
    do
    {
        pepolldata=pev->data.ptr;
        pepolldata->disconncall(pepolldata->extptr,pepolldata->ipstr);
        free(pepolldata->epollbuf);
        free(pepolldata->recvbuf);
        free(pepolldata->extptr);
        free(pepolldata->ipstr);
        free(pepolldata);
    }while(false);
}

void epollworkrecvdata(struct epoll_event *pev)
{
    int eax;
    uint tmp,crc;
    pepolldatainfo pepolldata;
    pepolldata=(pepolldatainfo)pev->data.ptr;
    do
    {
         if(pepolldata->flags & EPOLLFLAGS_RECVHEAD)
         {
            eax=recv(pepolldata->sockfd,pepolldata->epollbuf,pepolldata->epolllen,0);
            if(eax>0)
            {
                tmp=((uint)pepolldata->recvbuf)+pepolldata->epollseek;
                memmove((void *)tmp,pepolldata->epollbuf,eax);
                pepolldata->epollseek+=eax;
                if(pepolldata->epollseek==pepolldata->recvlength)
                {
                    tmp=*((uint*)pepolldata->recvbuf+2);
                    crc=crc32(0,pepolldata->recvbuf,8);
                    if(crc==tmp)
                    {
                        pepolldata->tag=*((uint*)pepolldata->recvbuf);
                        pepolldata->recvlength=*((uint*)pepolldata->recvbuf+1);
                        pepolldata->epolllen=pepolldata->recvlength;
                        pepolldata->epollseek=0;
                        pepolldata->epollbuf=realloc(pepolldata->epollbuf,pepolldata->recvlength);
                        pepolldata->recvbuf=realloc(pepolldata->recvbuf,pepolldata->recvlength);
                        pepolldata->flags &=~EPOLLFLAGS_RECVHEAD;
                        pepolldata->flags|=EPOLLFLAGS_RECVDATA;
                    }     
                    else
                    {
                        pepolldata->flags|=EPOLLFLAGS_SOCKERROR;
                        printf("recvheadcrc32error\n");
                        break;
                    }
                }
                else
                {
                    tmp=pepolldata->recvlength-pepolldata->epollseek;
                    pepolldata->epolllen=tmp;
                }
            }
            else
            {
                if (eax==0)
                {
                    pepolldata->flags|=EPOLLFLAGS_SOCKERROR;
                    break;
                }
                else if(eax==-1 && errno==EAGAIN)
                {
                    break;
                }
                else if(eax==-1 && errno!=EAGAIN)
                {    
                    pepolldata->flags|=EPOLLFLAGS_SOCKERROR;
                    printf("recvheaderrno:%d:%s\n",errno,strerror(errno));
                    break;
                }
            }
        }
        if(pepolldata->flags & EPOLLFLAGS_RECVDATA)
        {
            eax=recv(pepolldata->sockfd,pepolldata->epollbuf,pepolldata->epolllen,0);
            if(eax>0)
            {
                tmp=((uint)pepolldata->recvbuf)+pepolldata->epollseek;
                memmove((void *)tmp,pepolldata->epollbuf,eax);
                pepolldata->epollseek+=eax;
                if(pepolldata->epollseek==pepolldata->recvlength)
                {
                    if(pepolldata->tag==0)
                    {
                        pthread_mutex_lock(&epollheartbeatmutex);
                        pepolldata->heartbeatcount++;
                        pthread_mutex_unlock(&epollheartbeatmutex);
                    }
                    else
                    {
                        ////////////////////////////////////////////
                        pepollrecvqueuedatainfo precvqueuedata;
                        precvqueuedata=calloc(1,sizeof(epollrecvqueuedatainfo));
                        precvqueuedata->tag=pepolldata->tag;
                        precvqueuedata->recvlength=pepolldata->recvlength;
                        precvqueuedata->recvbuf=calloc(1,pepolldata->recvlength);
                        memmove(precvqueuedata->recvbuf,pepolldata->recvbuf,pepolldata->recvlength);
                        precvqueuedata->extptr=pepolldata->extptr;
                        precvqueuedata->ipstr=pepolldata->ipstr;
                        memmove(&precvqueuedata->ev,pev,sizeof(struct epoll_event));
                        precvqueuedata->recvcall=pepolldata->recvcall;
                        pthread_mutex_lock(&epollrecvqueuemutex);
                        InsertTailList(&epollrecvqueuelisthead,&precvqueuedata->recvqueuelist);
                        pthread_mutex_unlock(&epollrecvqueuemutex);
                        sem_post(pepollrecvqueuesem);
                        ///////////////////////////////////////////
                    }
                    pepolldata->epollseek=0;
                    pepolldata->epolllen=12;
                    pepolldata->recvlength=12;
                    pepolldata->epollbuf=realloc(pepolldata->epollbuf,pepolldata->recvlength);
                    pepolldata->recvbuf=realloc(pepolldata->recvbuf,pepolldata->recvlength);
                    pepolldata->flags &=~EPOLLFLAGS_RECVDATA;
                    pepolldata->flags|=EPOLLFLAGS_RECVHEAD;
                }
                else
                {
                    tmp=pepolldata->recvlength-pepolldata->epollseek;
                    pepolldata->epolllen=tmp;
                }
            }
            else
            {
                if (eax==0)
                {
                    pepolldata->flags|=EPOLLFLAGS_SOCKERROR;
                    break;
                }
                else if(eax==-1 && errno==EAGAIN)
                {
                    break;
                }
                else if(eax==-1 && errno!=EAGAIN)
                {
                    pepolldata->flags|=EPOLLFLAGS_SOCKERROR;
                    printf("recvdataerrno:%d:%s\n",errno,strerror(errno));
                    break;
                }
            }
        }
    }while(true);
}

void epollworksenddata(struct epoll_event *pev)
{
    int eax;
    uint tmp;
    pepolldatainfo pepolldata;
    pepolldata=(pepolldatainfo)pev->data.ptr;
    do
    {
        PLIST_ENTRY plist = NULL;
        pepollsenddatainfo pepollsenddata;         
        plist=pepolldata->sendlisthead.Flink;
        if(plist!=&pepolldata->sendlisthead)
        {
            pepollsenddata=(pepollsenddatainfo)plist;
            tmp=((uint)pepollsenddata->epollbuf)+pepollsenddata->epollseek;
            eax=send(pepolldata->sockfd,(void*)tmp,pepollsenddata->epolllen,MSG_NOSIGNAL);
            if (eax>0)
            {
                pepollsenddata->epollseek+=eax;
                if(pepollsenddata->epollseek==pepollsenddata->sendlength)
                {
                    free(pepollsenddata->epollbuf);
                    pthread_mutex_lock(&epollsendmutex);
                    RemoveEntryList(plist);
                    pthread_mutex_unlock(&epollsendmutex);
                    free(pepollsenddata);
                }
                else
                {
                   tmp=pepollsenddata->sendlength-pepollsenddata->epollseek;
                   pepollsenddata->epolllen=tmp;
                }
            }
            else
            {
                if(eax==-1 && errno==EAGAIN)
                {
                    break;
                }
                else if(eax==-1 && errno!=EAGAIN)
                {
                    pepolldata->flags|=EPOLLFLAGS_SOCKERROR;
                    printf("senderrno:%d:%s\n",errno,strerror(errno));
                    break;
                }
            }
        }
        else
        {
            break;
        }
    }while(true);
}

void epollworklisten(struct epoll_event *pev)
{
    int newsfd,len,eax;
    struct sockaddr_in clisa;
    void *pipstr;
    struct epoll_event ev;
    pepolldatainfo pepolldata;
    pepolldatainfo ptmpepolldata;
    pepolldata=(pepolldatainfo)pev->data.ptr;
    len=sizeof(struct sockaddr_in);
    memset(&clisa,0,sizeof(struct sockaddr_in));
    do
    {
        if((newsfd=accept(pepolldata->sockfd,(struct sockaddr *)&clisa,(socklen_t *)&len))>0)
        {
            eax=epollsetnonblocking(newsfd);
            memset(&ev,0,sizeof(struct epoll_event));
            pipstr=calloc(1,32);
            sprintf(pipstr,"%s:",inet_ntoa(clisa.sin_addr));
            sprintf((void*)((uint)pipstr+strlen(pipstr)),"%d",ntohs(clisa.sin_port));
            ptmpepolldata=calloc(1,sizeof(epolldatainfo));
            ptmpepolldata->heartbeatcount=2;
            ptmpepolldata->sockfd=newsfd;
            ptmpepolldata->flags |=EPOLLFLAGS_RECVHEAD;
            ptmpepolldata->epollseek=0;
            ptmpepolldata->epollbuf=calloc(1,12);
            ptmpepolldata->epolllen=12;
            ptmpepolldata->recvbuf=calloc(1,12);
            ptmpepolldata->recvlength=12;
            InitializeListHead(&ptmpepolldata->sendlisthead);
            ptmpepolldata->ipstr=pipstr;
            ptmpepolldata->extptr=calloc(1,pepolldata->epolllen);
            ptmpepolldata->recvcall=pepolldata->recvcall;
            ptmpepolldata->disconncall=pepolldata->disconncall;
            ev.data.ptr=ptmpepolldata;
            ev.events=EPOLLIN | EPOLLET | EPOLLERR | EPOLLHUP;
            eax=epoll_ctl(epollfd,EPOLL_CTL_ADD,newsfd,&ev);
            pthread_mutex_lock(&epollfreemutex);
            InsertTailList(&epollheartbeatlisthead,&ptmpepolldata->heartbeatlist);
            pthread_mutex_unlock(&epollfreemutex);
        }
        else
        {
            if(newsfd==-1 && errno==EAGAIN)
            {
                break;
            }
            else if(newsfd==-1 && errno != EAGAIN && errno != ECONNABORTED && errno != EPROTO && errno != EINTR)
            {
                printf("accept:%d%s\n",errno,strerror(errno));
                exit(0);
            }
        }
    }while(true);
}

void *epollrecvqueuethread(void *param)
{
    PLIST_ENTRY plist = NULL;
    pepollrecvqueuedatainfo precvqueuedata;
    while(true)
    {
        sem_wait(pepollrecvqueuesem);
        plist=epollrecvqueuelisthead.Flink;
        if(plist!=&epollrecvqueuelisthead)
        {
            precvqueuedata=(pepollrecvqueuedatainfo)plist;
            if(precvqueuedata->tag!=0)
            {
                precvqueuedata->recvcall(precvqueuedata->tag,precvqueuedata->recvbuf,precvqueuedata->recvlength,
                precvqueuedata->extptr,precvqueuedata->ipstr,&precvqueuedata->ev);
                pthread_mutex_lock(&epollrecvqueuemutex);
                RemoveEntryList(plist);
                pthread_mutex_unlock(&epollrecvqueuemutex);
                free(precvqueuedata->recvbuf);
                free(precvqueuedata);
            }
            else
            {
                epollworkfree(&precvqueuedata->ev);
                pthread_mutex_lock(&epollrecvqueuemutex);
                RemoveEntryList(plist);
                pthread_mutex_unlock(&epollrecvqueuemutex);
                free(precvqueuedata);
            }
        }
    }
}

void *epollworkthread(void * param)
{
    int fds=0,i=0;
    while(true)
    {
        fds=epoll_wait(epollfd,pepollevents,epollmaxevents,-1);
        #pragma omp parallel for
        for(i=0;i<fds;i++)
        {
            pepolldatainfo pepolldata=(pepolldatainfo)pepollevents[i].data.ptr;
            if(pepollevents[i].events & EPOLLERR || pepollevents[i].events & EPOLLHUP)
            {
                pepolldata->flags|=EPOLLFLAGS_SOCKERROR;
            }
            if((pepollevents[i].events & EPOLLIN) && !(pepolldata->flags & EPOLLFLAGS_SOCKERROR) 
                    && !(pepolldata->closestatus & EPOLLFLAGS_CLOSESTATUS) 
                    && (pepolldata->flags & EPOLLFLAGS_SOCKLISTEN))
            {
                epollworklisten(&pepollevents[i]);
            }
            else if((pepollevents[i].events & EPOLLIN) && !(pepolldata->flags & EPOLLFLAGS_SOCKERROR) 
                    && !(pepolldata->closestatus & EPOLLFLAGS_CLOSESTATUS) 
                    && !(pepolldata->flags & EPOLLFLAGS_SOCKLISTEN))
            {
                epollworkrecvdata(&pepollevents[i]);
            }
            if((pepollevents[i].events & EPOLLOUT) && !(pepolldata->flags & EPOLLFLAGS_SOCKERROR) 
                   && !(pepolldata->closestatus & EPOLLFLAGS_CLOSESTATUS))
            {
                epollworksenddata(&pepollevents[i]);
            }
            if((pepolldata->flags & EPOLLFLAGS_SOCKERROR) || (pepolldata->closestatus & EPOLLFLAGS_CLOSESTATUS)) 
            {
                if(!(pepolldata->flags & EPOLLFLAGS_SOCKFREE) && !(pepolldata->flags & EPOLLFLAGS_SOCKLISTEN))
                {
                    pepollrecvqueuedatainfo precvqueuedata;
                    PLIST_ENTRY plist = NULL;
                    pepolldata->flags|=EPOLLFLAGS_SOCKERROR;
                    pepolldata->flags|=EPOLLFLAGS_SOCKFREE;
                    epoll_ctl(epollfd,EPOLL_CTL_DEL,pepolldata->sockfd,&pepollevents[i]);
                    shutdown(pepolldata->sockfd,SHUT_RDWR);
                    close(pepolldata->sockfd);
                    plist=(PLIST_ENTRY)pepolldata;
                    pthread_mutex_lock(&epollfreemutex);
                    RemoveEntryList(plist);
                    pthread_mutex_unlock(&epollfreemutex);
                    pthread_mutex_lock(&epollsendmutex);
                    plist=pepolldata->sendlisthead.Flink;
                    while(plist!=&pepolldata->sendlisthead)
                    {
                        pepollsenddatainfo pepollsenddata;
                        pepollsenddata=(pepollsenddatainfo)plist;
                        free(pepollsenddata->epollbuf);
                        RemoveEntryList(plist);
                        plist=plist->Flink;
                        free(pepollsenddata);
                    }
                    pthread_mutex_unlock(&epollsendmutex);
                    precvqueuedata=calloc(1,sizeof(epollrecvqueuedatainfo));
                    precvqueuedata->tag=0;
                    memmove(&precvqueuedata->ev,&pepollevents[i],sizeof(struct epoll_event));
                    pthread_mutex_lock(&epollrecvqueuemutex);
                    InsertTailList(&epollrecvqueuelisthead,&precvqueuedata->recvqueuelist);
                    pthread_mutex_unlock(&epollrecvqueuemutex);
                    sem_post(pepollrecvqueuesem);
                }
                else
                {
                    printf("listensocketfree:%d:%s\n",errno,strerror(errno));
                }
            }
        }
    }
    pthread_exit((void*)0);
}

int epollheartbeatsend(uint tag,void*buf,uint bufsize,struct epoll_event *pev)
{
    pepolldatainfo pepolldata;
    pepollsenddatainfo pepollsenddata;
    void *sendbuf;
    do{
        if((pepolldata=pev->data.ptr)==NULL || bufsize==0)break;
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

//心跳检测，15分钟内无回应，断开连接。
void *epollheartbeatthread(void *param)
{
    PLIST_ENTRY plist;
    pepolldatainfo pepolldata;
    char buf[32]={0};
    struct epoll_event ev;
    while(true)
    {
        sleep(300);
        pthread_mutex_lock(&epollfreemutex);
        plist=epollheartbeatlisthead.Flink;
        while(plist!=&epollheartbeatlisthead)
        {
            pepolldata=(pepolldatainfo)plist;
            if (!(pepolldata->flags & EPOLLFLAGS_SOCKERROR))
            {
                if(pepolldata->heartbeatcount>0)
                {
                    pthread_mutex_lock(&epollheartbeatmutex);
                    pepolldata->heartbeatcount--;
                    pthread_mutex_unlock(&epollheartbeatmutex);
                    ev.data.ptr=pepolldata;
                    epollheartbeatsend(0,buf,4,&ev);
                }
                else
                {
                    pepolldata->closestatus|=EPOLLFLAGS_CLOSESTATUS;
                    ev.data.ptr=pepolldata;
                    ev.events=EPOLLIN | EPOLLOUT | EPOLLET | EPOLLERR | EPOLLHUP;
                    epoll_ctl(epollfd,EPOLL_CTL_MOD,pepolldata->sockfd,&ev);
                }
            }
            plist=plist->Flink;
        }
        pthread_mutex_unlock(&epollfreemutex);
    }
}