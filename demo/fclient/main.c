/* 
 * File:   main.c
 * Author: kkk
 *
 * Created on 2013年10月12日, 下午2:37
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <pthread.h>
#include "epollsocket.h"
#include <gtk/gtk.h>
#include <sys/stat.h>

typedef struct _sendwindowdata
{
    char *filename;
    char *filepath;
    uint filesize;
    uint tmpsize;
    uint seek;
    int fd;
    GtkWidget *ProgressBar;
    void *filebuf;
    
}sendwindowdata,*psendwindowdata;

GtkWidget *filechooserbutton1;

gboolean idle_callback(gpointer data)
{
    char tmpbuf[512];
    GtkWidget *ProgressBar;
    uint tmpsize,seek;
    ProgressBar=(GtkWidget*)(*((uint*)data));
    tmpsize=*((uint*)data+1);
    seek=*((uint*)data+2);
    memset(tmpbuf,0,512);
    sprintf(tmpbuf,"%d / %d",seek,tmpsize);
    gtk_progress_bar_set_text (GTK_PROGRESS_BAR(ProgressBar),tmpbuf);
    free(data);
    return FALSE;
}

void recvcall(uint tag,void *buf ,uint bufsize,void *extptr,void*ipstr,struct epoll_event *pev)
{
    char tmpbuf[512];
    uint tmp;
    void *pbuf;
    psendwindowdata pswd;
    pswd=(psendwindowdata)extptr;
    switch(tag)
    {
        case 2: //只是为了演示，每次按8192字节发送
            if(pswd->filesize>8192)
            {
                tmp=(uint)pswd->filebuf+pswd->seek;
                epollsocketsend(2,(void*)tmp,8192,pev);
                pswd->seek+=8192;
                pswd->filesize-=8192;
                pbuf=calloc(1,12);
                *((uint*)pbuf)=(uint)pswd->ProgressBar;
                *((uint*)pbuf+1)=pswd->tmpsize;
                *((uint*)pbuf+2)=pswd->seek;
                g_idle_add(idle_callback,pbuf);
            }
            else if(pswd->filesize>0)
            {
                tmp=(uint)pswd->filebuf+pswd->seek;
                epollsocketsend(2,(void*)tmp,pswd->filesize,pev);
                pswd->seek+=pswd->filesize;
                pswd->filesize-=pswd->filesize;
                pbuf=calloc(1,12);
                *((uint*)pbuf)=(uint)pswd->ProgressBar;
                *((uint*)pbuf+1)=pswd->tmpsize;
                *((uint*)pbuf+2)=pswd->seek;
                g_idle_add(idle_callback,pbuf);
            }
            else
            {
                free(pswd->filebuf);
                epollsocketsend(3,tmpbuf,4,pev);
            }
            break;
        default:
            break;
    }

}

void disconncall(void*extptr,void*ipstr)
{

}

void createsendwindow(psendwindowdata pswd)
{
    GtkWidget *window,*fixed;
    GtkWidget *label1,*ProgressBar;
    struct stat st;
    struct epoll_event ev;
    void *extptr;
    char tmpbuf[120];
    window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
    fixed=gtk_fixed_new();
    gtk_window_resize(GTK_WINDOW(window),200,150);
    gtk_container_add (GTK_CONTAINER (window), fixed);
    label1=gtk_label_new("0");
    ProgressBar=gtk_progress_bar_new();
    gtk_window_set_default_size(GTK_WINDOW(label1),170,37);
    gtk_window_set_default_size(GTK_WINDOW(ProgressBar),370,37);
    gtk_widget_set_uposition(ProgressBar,0,37);
    gtk_container_add (GTK_CONTAINER (fixed), label1);
    gtk_container_add (GTK_CONTAINER (fixed), ProgressBar);
    gtk_widget_show_all(window);
    pswd->ProgressBar=ProgressBar;
    gtk_label_set_text(GTK_LABEL(label1),pswd->filename);
    pswd->fd=open(pswd->filepath,O_RDONLY);
    fstat(pswd->fd,&st);
    pswd->filesize=st.st_size;
    pswd->filebuf=calloc(1,pswd->filesize);
    read(pswd->fd,pswd->filebuf,pswd->filesize);
    pswd->tmpsize=st.st_size;
    extptr=epollsocketconnect("127.0.0.1",17777,sizeof(sendwindowdata),&ev,recvcall,disconncall);
    memmove(extptr,pswd,sizeof(sendwindowdata));
    free(pswd);
    memset(tmpbuf,0,120);
    strcat(tmpbuf,((psendwindowdata)extptr)->filename);
    epollsocketsend(1,tmpbuf,120,&ev);
}

void window_destroy (GtkObject *object, gpointer user_data)
{
    gtk_main_quit ();
}

void button1_clicked(GtkButton *button,gpointer data)
{
    GtkWidget *dialog;
    gchar *str,*tmp,*strstr;
    psendwindowdata pswd;
    if((str=gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(filechooserbutton1)))!=NULL)
    {
        strstr=calloc(1,512);
        memmove(strstr,str,strlen(str));
        tmp=strrchr(strstr,'/');
        tmp++;
        pswd=calloc(1,sizeof(sendwindowdata));
        pswd->filename=tmp;
        pswd->filepath=strstr;
        createsendwindow(pswd);
    }
}

int main(int argc, char** argv) {
    GtkWidget *window;
    GtkBuilder      *builder; 
    gtk_init (&argc, &argv);
    epollsocketinit(1000);
    builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, "fclient.glade", NULL);
    window = GTK_WIDGET (gtk_builder_get_object (builder, "window1"));
    filechooserbutton1=GTK_WIDGET (gtk_builder_get_object (builder, "filechooserbutton1"));
    gtk_builder_connect_signals (builder, NULL);
    g_object_unref (G_OBJECT (builder));
    gtk_widget_show (window);
    gtk_main ();

    return (EXIT_SUCCESS);
}

