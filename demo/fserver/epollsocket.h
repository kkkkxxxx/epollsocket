
#ifndef EPSOCKET_H
#define	EPSOCKET_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/epoll.h>


//tag：要发送的标签说明数据类型>0
//buf：要发送的数据缓冲区
//bufsize：要发送的数据大小>0
//pev：需要一个struct epoll_event *（可以看成是一个socket描述符)
//返回值：成功返回0,失败返回-1    
int epollsocketsend(uint tag,void*buf,uint bufsize,struct epoll_event *pev);

//pev：需要一个struct epoll_event *（可以看成是一个socket描述符)
void epollsocketclose(struct epoll_event *pev);

//port：要监听的端口
//extsize：扩展数据大小>0
//void (*recvcall)(uint tag,void *buf ,uint bufsize,void *extptr,void*ipstr,struct epoll_event *pev)
//recvcall函数指针，当收到数据调用此函数。
    //tag：接收到的标签说明数据类型
    //buf：接收到的数据缓冲区
    //bufsize：接收到的数据缓冲区大小
    //extprt：扩展数据的指针（由epolllisten函数extsize指定extptr大小)
    //ipstr：IP地址端口字符串
    //pev：不要更改此pev，该参数用于epollsocketsend/epollsocketclose调用（可以看成是一个socket描述符)
   //如果需要在其他地方调用epollsocketsend发送数据，应该复制保存pev
//void(*disconncall)(void*extptr,void*ipstr))
//disconncall函数指针，当有连接断开调用此函数
    //extprt：扩展数据的指针（由epolllisten函数extsize指定extptr大小)
    //ipstr：IP地址端口字符串
//返回值：成功返回0，失败返回-1
int epollsocketlisten(ushort port,uint extsize,
        void (*recvcall)(uint tag,void *buf ,uint bufsize,void *extptr,void*ipstr,struct epoll_event *pev),
        void(*disconncall)(void*extptr,void*ipstr));



//addrstr:要连接的IP地址或者域名
//port：要连接的端口
//extsize：扩展数据大小>0
//pev：需要一个struct epoll_event *（可以看成是一个socket描述符)
//void (*recvcall)(uint tag,void *buf ,uint bufsize,void *extptr,void*ipstr,struct epoll_event *pev)
//recvcall函数指针，当收到数据调用此函数。
    //tag：接收到的标签说明数据类型
    //buf：接收到的数据缓冲区
    //bufsize：接收到的数据缓冲区大小
    //extprt：扩展数据的指针（由epolllisten函数extsize指定extptr大小)
    //ipstr：IP地址端口字符串
    //pev：不要更改此pev，该参数用于epollsocketsend/epollsocketclose调用（可以看成是一个socket描述符)
   //如果需要在其他地方调用epollsocketsend发送数据，应该复制保存pev
//void(*disconncall)(void*extptr,void*ipstr))
//disconncall函数指针，当有连接断开调用此函数
    //extprt：扩展数据的指针（由epolllisten函数extsize指定extptr大小)
    //ipstr：IP地址端口字符串
//返回值：成功返回扩展数据指针extprt，失败返回NULL

//struct epoll_event *pev;

//int main(int argc, char** argv) {
//    void *extbuf;
//    struct epoll_event ev;
//    extbuf=epollsocketconnect("127.0.0.1",15000,100,&ev,recvcall,disconncall);
//    if (extbuf!=NULL)//不等于NULL连接成功
//    {
//        epollsocketsend(3,buf,512,&ev);
//        //如果需要在其他地方调用epollsocketsend发送数据，应该复制保存ev
//        pev=malloc(sizeof(struct epoll_event));
//        memmove(pev,&ev,sizeof(struct epoll_event));
//        epollsocketsend(3,buf,512,pev);
//    }
//    return (EXIT_SUCCESS);
//}
void* epollsocketconnect(char* addrstr,ushort port,uint extsize,struct epoll_event *pev,
         void (*recvcall)(uint tag,void *buf ,uint bufsize,void *extptr,void*ipstr,struct epoll_event *pev),
         void(*disconncall)(void*extptr,void*ipstr));

//maxconn：最大连接数
//返回值：成功返回0,失败返回-1
int epollsocketinit(int maxconn);

#ifdef	__cplusplus
}
#endif

#endif	/* EPSOCKET_H */

