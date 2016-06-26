
#ifndef EPWORKTHREAD_H
#define	EPWORKTHREAD_H

#ifdef	__cplusplus
extern "C" {
#endif

void *epollrecvqueuethread(void *param);
    
void *epollworkthread(void *param);

void *epollheartbeatthread(void *param);

#ifdef	__cplusplus
}
#endif

#endif	/* EPWORKTHREAD_H */

