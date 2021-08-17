#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include "queue.h"

#ifdef __WIN32__
#include <winsock2.h>
#include <afunix.h>
#define GETSOCKETERRNO() (WSAGetLastError())

#else
#include <sys/socket.h>
#include <sys/un.h>
#define GETSOCKETERRNO() (errno)
#include <errno.h>
#endif



#define UNIX_PATH_MAX 108 /* man 7 unix */
#define SOCKNAME "socket.skt"
#define N 100
Task_queue tasks;
struct Config
{
    int thread_n; //numero di thread workers
    int capacity; //capacità di storage 
    int max_files; //massimo numero di file nella memoria
    char * socketfile_name; //nome del socket file
    char * log_file; //nome del file di log
    int max_pending_conn; //numero massimo di connessioni pendenti
};

struct Config * config = NULL;

void parse_line(char * line)
{
    if(config == NULL || line == NULL || strlen(line)<=2)
        return;

    const char* sep = "=";
    char *token, *saveptr;
    token = strtok_r(line, sep, &saveptr);
  
    if(strcmp(token, "thread_n")==0) {
        config->thread_n = atoi(strtok_r(NULL, sep, &saveptr));

    }
    else if(strcmp(token, "capacity")==0) {
        config->capacity = atoi(strtok_r(NULL, sep, &saveptr));

    }    
    else if(strcmp(token, "socketfile_name")==0) {
        config->socketfile_name = (char *) malloc(100*sizeof(char));
        strcpy(config->socketfile_name, strtok_r(NULL, sep, &saveptr));

    } 
    else if(strcmp(token, "max_files")==0) {
        config->max_files = atoi(strtok_r(NULL, sep, &saveptr));

    } 
    else if(strcmp(token, "log_file")==0) {
        config->log_file= (char *) malloc(100*sizeof(char));
        strcpy(config->log_file, strtok_r(NULL, sep, &saveptr));
    } 
    else if(strcmp(token, "max_pending_conn")==0) {
        config->max_pending_conn = atoi(strtok_r(NULL, sep, &saveptr));
    }
}

void read_config_file ( char * config_name){
    FILE * fp;
    char  line[64];
    if(( fp = fopen(config_name, "rb"))==NULL) {
		exit(1);
	}
    
 	while(!feof(fp)){
	   	fgets(line, 64, fp);
        if(line[0] != '\n')
            parse_line(line);

      }
	fclose(fp);
}



void run_server (int fd_skt, struct sockaddr_un  server){
    int fd_c;
    printf("%d\n", errno);

    listen(fd_skt, N); //N è il numero massimo di connessioni pendenti, da valutare
    
    unsigned int size = sizeof(server);
    printf("%d\n", errno);
    errno = 0;
        while (1){
            fd_c = accept(fd_skt, (struct sockaddr*) &server, &size);
            printf("%d\n", errno);
            //printf ("fd_c = %d , fd_skt = %d\n", fd_c, fd_skt);
            //fd_c=accept(fd_skt,NULL,0);
            //parte di esempio
            char buf[N];
            read(fd_c,buf,N);
            printf("Server got: %s\n",buf);
            write(fd_c,"Bye!",5);
            close(fd_skt); 
            close(fd_c);
            exit(EXIT_SUCCESS); 
        }
    
}

int main (){
    
    
    int fd_skt;
    struct sockaddr_un  server ;
    config = (struct Config *) malloc (sizeof (struct Config));
    read_config_file("config.txt");
    printf("LETTURA CONFIG.TXT\n");
    printf("*numero di thread : %d\n", config->thread_n);
    printf("*capacità di storage : %d\n", config->capacity);
    printf("*nome del socketfile : %s\n", config->socketfile_name);
    printf("*numero massimo di file memorizzabili : %d\n", config->max_files);
    printf("*nome del file di log : %s\n", config->log_file);
    printf("*numero massimo di connessioni pendenti : %d\n", config->max_pending_conn);


    
    
 
    //creo il socket
    unlink(SOCKNAME);
    
    fd_skt = socket(AF_UNIX,SOCK_STREAM, 0);
    memset((void *) &server, '0', sizeof(server));


    server.sun_family = AF_UNIX;
    strncpy(server.sun_path, SOCKNAME, strlen(SOCKNAME)+1);

    if(bind(fd_skt, (struct sockaddr *) &server, sizeof(server))<0){
        printf("bind fallita\n");
        printf("%d\n", errno);
        exit(EXIT_FAILURE);
    }
    

    run_server(fd_skt, server);
    printf("**FINE**\n");
    
    return 0;

}