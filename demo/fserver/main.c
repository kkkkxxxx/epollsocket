/* 
 * File:   main.c
 * Author: kkk
 *
 * Created on 2013年10月3日, 下午7:38
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <pthread.h>
#include "epollsocket.h"
#include <gtk/gtk.h>
#include <fcntl.h>

typedef struct _sendwindowdata
{
    char *filename;
    char *filepath;
    uint filesize;
    uint seek;
    int fd;
    GtkWidget *label3;
    void *filebuf;
    
}sendwindowdata,*psendwindowdata;

void recvcall(uint tag,void *buf ,uint bufsize,void *extptr,void*ipstr,struct epoll_event *pev)
{
    char tmpbuf[512];
    psendwindowdata pswd;
    pswd=(psendwindowdata)extptr;
    switch(tag)
    {
        case 1:
            memset(tmpbuf,0,512);
            strcat(tmpbuf,"/home/kkk/tmp/");
            strcat(tmpbuf,buf);
            pswd->fd=open(tmpbuf, O_CREAT |O_RDWR | O_TRUNC,0644);
            epollsocketsend(2,tmpbuf,4,pev);
            break;
        case 2:
            write(pswd->fd,buf,bufsize);
            epollsocketsend(2,tmpbuf,4,pev);
            break;
        case 3:
            close(pswd->fd);
            epollsocketclose(pev);
            epollsocketsend(2,tmpbuf,4,pev);
            break;
        default:
            break;
    }
}

void disconncall(void*extptr,void*ipstr)
{

}

void window_destroy (GtkObject *object, gpointer user_data)
{
    gtk_main_quit ();
}

int main(int argc, char** argv) {
    GtkWidget *window;
    GtkBuilder      *builder; 
    gtk_init (&argc, &argv);
    builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, "fserver.glade", NULL);
    window = GTK_WIDGET (gtk_builder_get_object (builder, "window1"));
    gtk_builder_connect_signals (builder, NULL);
    g_object_unref (G_OBJECT (builder));
    gtk_widget_show (window);
    epollsocketinit(100);
    epollsocketlisten(17777,sizeof(sendwindowdata),recvcall,disconncall);
    gtk_main ();
    return (EXIT_SUCCESS);
}

