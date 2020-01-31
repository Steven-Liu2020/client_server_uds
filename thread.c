#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "pthread.h"
#include "unistd.h"
#include "sys/socket.h"
#include "sys/un.h"
#include "stddef.h"
#include "fcntl.h"
#include "sys/select.h"
#include "aio.h"
#include "errno.h"
#include "signal.h"

#define BUFFER_SIZE 128
#define MAX_CONNECT_NUM 5

#define SOCK_S "sock_server"
#define SOCK_C "sock_client"

#define err_log(errlog) do{perror(errlog);exit(EXIT_FAILURE);}while(0)

void *server_thread(void *arg);
void *client_sync_block(void *arg);
void *client_sync_nonblock(void *arg);
void *client_async_block(void *arg);
void *client_async_nonblock(void *arg);
void aio_completion_handler(int signo,siginfo_t *info,void *context);

int main(int argc,char *argv[])
{
        pthread_t ser_id,cli_id;
        int ret1 = 0,ret2 = 0;
        void *rec1,*rec2;
        void *(*client_func)(void *);
        if (argc != 2)
                err_log("Please input correct parameter:(eg thread c1)");
        if (strcmp(argv[1],"c1") == 0)
                client_func = client_sync_block;
        else if (strcmp(argv[1],"c2") == 0)
                client_func = client_sync_nonblock;
        else if (strcmp(argv[1],"c3") == 0)
                client_func = client_async_block;
        else if (strcmp(argv[1],"c4") == 0)
                client_func = client_async_nonblock;
        else
                err_log("Please input correct parameter:(eg thread c1)");
        ret1 = pthread_create(&ser_id,NULL,server_thread,NULL);
        if (ret1 != 0)
                err_log("Server thread create failed");
        sleep(1);
        ret2 = pthread_create(&cli_id,NULL,client_func,NULL);
        if (ret2 != 0)
                err_log("Client thread create failed");

        ret2 = pthread_join(cli_id,&rec2);
        if (ret2 != 0)
                err_log("Client thread exit failed");
        ret1 = pthread_join(ser_id,&rec1);
        if (ret1 != 0)
                err_log("Server thread exit failed");
        return 0;
}

void *server_thread(void *arg)
{
        int ser_fd,cli_fd,ret;
        int recv_bytes,send_bytes;
        struct sockaddr_un un;
        char recv_buff[BUFFER_SIZE],send_buff[BUFFER_SIZE];
        int reuse_val = 1;
        memset(&un, 0, sizeof(un));
        un.sun_family = AF_UNIX;
        unlink(SOCK_S);
        strcpy(un.sun_path,SOCK_S);

        ser_fd = socket(AF_UNIX,SOCK_STREAM,0);
        if (ser_fd < 0)
                err_log("Server socket() failed");
        //Set addr reuse
        setsockopt(ser_fd,SOL_SOCKET,SO_REUSEADDR,(void *)&reuse_val,sizeof(reuse_val));
        ret = bind(ser_fd,(struct sockaddr *)&un,sizeof(un));
        if (ret < 0)
                err_log("Server bind() failed");
        ret = listen(ser_fd,MAX_CONNECT_NUM);
        if (ret < 0)
                err_log("Srevet listen() failed");
        while (1){
                struct sockaddr_un client_addr;
                cli_fd = accept(ser_fd,NULL,NULL);
                if (cli_fd < 0)
                        continue;
                memset(recv_buff,0,BUFFER_SIZE);
                recv_bytes = recv(cli_fd,recv_buff,BUFFER_SIZE,0);
                if (recv_bytes <= 0)
                        err_log("Server recv() failed");
                recv_buff[recv_bytes] = '\0';
                printf("Server recv: %s\n",recv_buff);
                sleep(3);
                *send_buff = '\0';
                strcat(send_buff,"welcome");
                printf("Server send: %s\n",send_buff);
                send_bytes = send(cli_fd,send_buff,strlen(send_buff),0);
                if (send_bytes < 0)
                        err_log("Server send() failed");
                close(cli_fd);
        }
        close(ser_fd);
        return NULL;
}

void *client_sync_block(void *arg)//Client Synchronous Blocking
{
        int fd,ret,recv_bytes,send_bytes;
        struct sockaddr_un ser_addr;
        char recv_buff[BUFFER_SIZE],send_buff[BUFFER_SIZE];
        memset(&ser_addr,0,sizeof(ser_addr));
        ser_addr.sun_family = AF_UNIX;
        strcpy(ser_addr.sun_path,SOCK_S);

        fd = socket(AF_UNIX,SOCK_STREAM,0);
        if (fd < 0)
                err_log("Client socket() failed");
        ret = connect(fd,(struct sockaddr *)&ser_addr,sizeof(ser_addr));
        if (ret < 0)
                err_log("Client connent() failed");
        memset(send_buff,0,sizeof(send_buff));
        strcpy(send_buff,"I am synchronous block");
        printf("Client send: %s\n",send_buff);
        send_bytes = send(fd,send_buff,BUFFER_SIZE,0);
        if (send_bytes < 0)
                err_log("Client send() failed");
        memset(recv_buff,0,sizeof(recv_buff));
        recv_bytes = recv(fd,recv_buff,BUFFER_SIZE,0);
        if (recv_bytes < 0)
                err_log("Client recv() failed");
        recv_buff[recv_bytes] = '\0';
        printf("Client recv: %s\n",recv_buff);
        close(fd);
        return NULL;
}

void *client_sync_nonblock(void *arg) //Client Synchronous Non-blocking
{
        int fd,ret,recv_bytes;
        struct sockaddr_un ser_addr;
        char recv_buff[BUFFER_SIZE],send_buff[BUFFER_SIZE];
        int flags;
        memset(&ser_addr,0,sizeof(ser_addr));
        ser_addr.sun_family = AF_UNIX;
        strcpy(ser_addr.sun_path,SOCK_S);

        fd = socket(AF_UNIX,SOCK_STREAM,0);
        if (fd < 0)
                err_log("Client2 socket() failed");
        
        flags = fcntl(fd,F_GETFL,0); //setting socket to nonblock
        fcntl(fd,flags|O_NONBLOCK);
        
        ret = connect(fd,(struct sockaddr *)&ser_addr,sizeof(ser_addr));
        if (ret < 0)
                err_log("Client2 connect failed");
        memset(send_buff,0,BUFFER_SIZE);
        strcpy(send_buff,"I am synchronous nonblock");
        printf("Client send: %s\n",send_buff);
        while (send(fd,send_buff,BUFFER_SIZE,MSG_DONTWAIT) < 0){
                printf("Client sending ...\n");
                sleep(1);
        }
        memset(recv_buff,0,BUFFER_SIZE);
        while ((recv_bytes = recv(fd,recv_buff,BUFFER_SIZE,MSG_DONTWAIT)) < 0){
                printf("Client recving ...\n");
                sleep(1);
        }
        recv_buff[recv_bytes] = '\0';
        printf("Client recv: %s\n",recv_buff);
        close(fd);
        return NULL;
}

void *client_async_block(void *arg) //Client Asynchronous Blocking
{
        int fd,ret,recv_bytes,send_bytes;
        struct sockaddr_un ser_addr;
        char recv_buff[BUFFER_SIZE],send_buff[BUFFER_SIZE];
       
        fd_set readset,writeset;
        
        memset(&ser_addr,0,sizeof(ser_addr));
        ser_addr.sun_family = AF_UNIX;
        strcpy(ser_addr.sun_path,SOCK_S);

        fd = socket(AF_UNIX,SOCK_STREAM,0);
        if (fd < 0)
                err_log("Client3 socket() failed");
        ret = connect(fd,(struct sockaddr *)&ser_addr,sizeof(ser_addr));
        if (ret < 0)
                err_log("Client3 connect() failed");
        memset(send_buff,0,BUFFER_SIZE);
        strcpy(send_buff,"I am asynchronous block");
        printf("Client send: %s\n",send_buff);
        send_bytes = send(fd,send_buff,BUFFER_SIZE,0);
        if (send_bytes < 0)
                err_log("Client3 send() failed");
        while (1){
                FD_ZERO(&readset);   //clear read set
                FD_SET(fd,&readset); //add descriptor
                ret = select((fd+1),&readset,NULL,NULL,NULL);//block
                if (ret < 0)
                        err_log("Client3 select() failed");
                else if(ret == 0)
                        break;
                else if(FD_ISSET(fd,&readset)){ 
                        memset(recv_buff,0,sizeof(recv_buff));
                        recv_bytes = recv(fd,recv_buff,BUFFER_SIZE,MSG_DONTWAIT);
                        recv_buff[recv_bytes] = '\0';
                        printf("Client recv: %s\n",recv_buff);

                        break;
                }
        }
        close(fd);
        return NULL;
}

void *client_async_nonblock(void *arg)//Client Asynchronous Non-blocking
{
        struct aiocb rd,wr;
        struct sigaction sig_act;
        int fd,ret,recv_bytes,send_bytes;
        char recv_buff[BUFFER_SIZE],send_buff[BUFFER_SIZE];
        struct sockaddr_un ser_addr;
        int i;

        memset(&ser_addr,0,sizeof(ser_addr));
        ser_addr.sun_family = AF_UNIX;
        strcpy(ser_addr.sun_path,SOCK_S);

        fd = socket(AF_UNIX,SOCK_STREAM,0);
        if (fd < 0)
                err_log("Client4 socket() failed");
        ret = connect(fd,(struct sockaddr *)&ser_addr,sizeof(ser_addr));
        if (ret < 0)
                err_log("Client4 connect() failed");
        memset(&wr,0,sizeof(wr));
        wr.aio_buf = send_buff;
        wr.aio_fildes = fd;
        wr.aio_nbytes = BUFFER_SIZE;
        
        memset(send_buff,0,BUFFER_SIZE);
        strcpy(send_buff,"I am asynchronous nonblock");
        printf("Client send: %s\n",(char *)(wr.aio_buf));
        ret = aio_write(&wr);
        if (ret < 0)
                err_log("Client4 aio_write failed");
        while (aio_error(&wr) == EINPROGRESS);
        ret = aio_return(&wr);

        sigemptyset(&sig_act.sa_mask);//Set up the signal handler
        sig_act.sa_flags = SA_SIGINFO;
        sig_act.sa_sigaction = aio_completion_handler;
        
        memset(&rd,0,sizeof(rd)); //Set up the AIO request
        rd.aio_buf = recv_buff;
        rd.aio_fildes = fd;
        rd.aio_nbytes = BUFFER_SIZE;
        rd.aio_offset = 0;
        //Link the AIO request with the signal handler
        rd.aio_sigevent.sigev_notify = SIGEV_SIGNAL;
        rd.aio_sigevent.sigev_signo = SIGIO;
        rd.aio_sigevent.sigev_value.sival_ptr = &rd;
        //Map the signal to the signal handler
        ret = sigaction(SIGIO,&sig_act,NULL);

        memset(recv_buff,0,sizeof(0));
        ret = aio_read(&rd);
        if (ret < 0)
                err_log("Client4 aio_read failed");
        //while (aio_error(&rd) == EINPROGRESS);
        //ret = aio_return(&rd);
        //printf("Client recv: %s\n",rd.aio_buf);
        for (i = 0; i < 5; ++i){
                printf("sleep...\n");
                sleep(1);
        }
        close(fd);
        return NULL;
}
void aio_completion_handler(int signo,siginfo_t *info,void *context)
{
        struct aiocb *req;
        int ret;
        //Ensure it's our signal
        if(info->si_signo == SIGIO){
                req = (struct aiocb *)info->si_value.sival_ptr;
                if(aio_error(req) == 0){
                        ret = aio_return(req);
                        printf("Client recv: %s\n",(char *)(req->aio_buf));
                }
        }
        return;
}
