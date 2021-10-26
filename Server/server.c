#define _GNU_SOURCE

#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include <signal.h>

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
#define READ_AND_BREAK(a,b,c)   if(!read(a,b,c)) break;
 
Task_queue tasks; //coda di task
File_queue * fs;  //file system
Config * config;
const int ONE = 1;
const int ZERO = 0;
const int MENOUNO = -1;
static volatile int keepRunning = 1;
int fd_skt;



void intHandler(int dummy) {
    printf("è arrivata una signal %d\n", dummy);
    if (dummy == 1)
        keepRunning = -1;
    if (dummy == 13 || dummy == 10)
        return;
    else {
        keepRunning = 0;
        shutdown(fd_skt, SHUT_RDWR);
        close(fd_skt);
    }
}

void parse_line(char * line) {
    if(config == NULL || line == NULL || strlen(line)<=2)
        return;
    const char* sep = "=\n";
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
        config->socketfile_name[strlen(config->socketfile_name)-1] = '\0';
    } 
    else if(strcmp(token, "max_files")==0) {
        config->max_files = atoi(strtok_r(NULL, sep, &saveptr));
    } 
    else if(strcmp(token, "log_file")==0) {
        config->log_file= (char *) malloc(100*sizeof(char));
        strcpy(config->log_file, strtok_r(NULL, sep, &saveptr));
        config->log_file[strlen(config->log_file)-1] = '\0';
    } 
    else if(strcmp(token, "max_pending_conn")==0) {
        config->max_pending_conn = atoi(strtok_r(NULL, sep, &saveptr));
    }
}

void read_config_file ( char * config_name){
    FILE * fp;
    char  line[100];
    if(( fp = fopen(config_name, "rb"))==NULL) {
        fprintf(log_file, "ERRORE NELLA LETTURA DEL FILE DI CONFIGURAZIONE %s\n", config_name);
		exit(1);
	}
 	while(fgets(line, 100, fp)){
        if(line[0] != '\0') {
            parse_line(line);
        }
      }
	fclose(fp);
}


void run_server ( struct sockaddr_un  server){
    
    int fd_c;

    listen(fd_skt, config->max_pending_conn);

    unsigned int size = sizeof(server);
        while (1){
            fd_c = accept(fd_skt, (struct sockaddr*) &server, &size);
            if (fd_c != -1) {
                fprintf(log_file, "\nÈ ARRIVATA UNA NUOVA CONNESIONE, CLIENT %d\n", fd_c);
            }

            if (keepRunning == 1) { 
                printf("nuova richiesta in esecuzione da %d\n", fd_c);
                push_task(fd_c, &tasks);
                pthread_cond_signal(&tasks.cond);
            }

            if (keepRunning == -1) {
                break;
            }
            
            else if (keepRunning == 0) { //È ARRIVATO UN SEGNALE DI TERMINAZIONE
                printf("SEGNALA AI THREAD LA TERMINAZIONE\n");
                pthread_cond_broadcast(&tasks.cond);
                pthread_cond_broadcast(&(fs->cond));
                break;
            }
        }    
}

/*CODICE CHE REALIZZA IL TASK 
(il carattere relativo all'operazione è già stato letto e viene passato come parametro)*/
int task_exec (int fd_c,  char c) {    //codice operazione
    fprintf(log_file, "THREAD WORKER IN ESECUZIONE %d\n",gettid());
    switch (c)
    {
       case 'o': //openFile
            {
                fprintf(log_file, "\nRICHIESTA DI APERTURA\n");
                int pathname_lenght, flags; //O_CREATE, O_LOCK
                
                READ_AND_BREAK(fd_c, &pathname_lenght, sizeof(int));
                char  pathname[256];
                READ_AND_BREAK(fd_c, pathname, pathname_lenght);
                READ_AND_BREAK(fd_c, &flags, sizeof(int));
                int check = 0;
                fprintf(log_file, "\tNOME DEL FILE : %s , FLAG = %d\n", pathname, flags);
                fprintf(log_file, "\t**LEGENDA DELLE FLAG : \n\t\t0 -> O_CREATE = 0 , O_LOCK = 0\n\t\t1 -> O_CREATE = 1 , O_LOCK = 0\n\t\t2 -> O_CREATE = 0 , O_LOCK = 1\n\t\t3 -> O_CREATE = 1 , O_LOCK = 1\n");
                
                //richiesta di lettura file
                if (flags == 0) { //O_CREATE = 0 , O_LOCK = 0
                    int ans = search_file(pathname, * fs);
                    if (ans == 1) {
                        check = 0;
                    }
                    else {
                        check = 2; //file non esistente
                    }
                }
                if (flags == 1) { //O_CREATE = 1 , O_LOCK = 0
                    check = search_file(pathname, * fs);
                    if (check == 0) {
                        push_new_file(pathname, 0, fs, config);
                    }
                    else 
                        check = 17;
                }
                if (flags == 2) { //O_CREATE = 0 , O_LOCK = 1
                    if (search_file(pathname, *fs) && set_locker(pathname, fd_c, fs)) {
                        check = 0;
                    }
                    else 
                        check = 4;
                }
            
                if (flags == 3) { //O_CREATE = 1 , O_LOCK = 1
                    if (search_file(pathname, * fs)) {
                        check = 1;
                    }
                    else {
                        check = 0;
                        push_new_file(pathname, fd_c, fs, config);
                    }
                }
                if (flags != 3) {
                }
                
                write(fd_c, &check, sizeof(int));

                if (check == 0) {
                    File_node * tmp = file_get(pathname, fs);
                    pthread_mutex_lock(&(fs->lock));
                    tmp->file_users = add_client(tmp->file_users, fd_c);
                    pthread_mutex_unlock(&(fs->lock));
                }
                fprintf(log_file, "\tRISULTATO OPERAZIONE : %d\n", check);
            }
            break;

        case 'r': //readFile
            {
                fprintf(log_file, "\nRICHIESTA DI LETTURA\n");
                int pathname_lenght;
                READ_AND_BREAK(fd_c, &pathname_lenght, sizeof(int)); 
                char pathname[256];
                READ_AND_BREAK(fd_c, pathname, pathname_lenght); //leggo il nome
                fprintf(log_file, "\tNOME DEL FILE : %s\n", pathname);
                File_node * tmp;
                if( (tmp = file_get(pathname, fs))) {//se il nome è stato trovato
                    pthread_mutex_lock(&(fs->lock));
                    if ((tmp->locker == 0 || tmp->locker == fd_c) && tmp->content) {
                        int check = 1;
                        int lenght = strlen(tmp->content) + 1;
                        write(fd_c, &check, sizeof(int));
                        write(fd_c, &lenght, sizeof(int)); 
                        write(fd_c, tmp->content, lenght);
                        fprintf(log_file, "\tBYTE LETTI: %d\n", lenght - 1);
                    }
                    else {
                        fprintf(log_file, "\tFILE NON TROVATO\n");
                        int check = -1;
                        write(fd_c, &check, sizeof(int));
                    }
                    pthread_mutex_unlock(&(fs->lock));                  
                }
                else {
                    int check = 0;
                    write(fd_c, &check, sizeof(int)); //altrimenti invio 0
                }
            }
            break;

        case 'R': //readNFile
            {
                int n_files;
                READ_AND_BREAK(fd_c, &n_files, sizeof(int)); //numero di file da leggere
                fprintf(log_file, "\nRICHIESTA DI LETTURA DI %d FILE\n", n_files);
                pthread_mutex_lock(&(fs->lock));
                int unlocked_files = unlocked_files_count(fd_c, *fs);
                if ( n_files > unlocked_files || n_files <= 0)
                    n_files = unlocked_files;
                write(fd_c, &n_files, sizeof(int)); //numero di file che il server invia
                fprintf(log_file, "\tIL SERVER HA %d FILE DA POTER INVIARE\n", n_files);
                File_node * tmp = fs-> first;
                int i = 0;
                while (i < n_files && tmp != NULL && keepRunning) {
                    if((tmp->locker == 0 || tmp->locker == fd_c) && tmp->content) {
                        int lenght = strlen(tmp->name) + 1;
                        write(fd_c, &lenght, sizeof(int));
                        write(fd_c, tmp->name, lenght);
                        int cont_lenght = strlen(tmp->content) + 1;
                        fprintf(log_file, "\tNOME DEL FILE : %s , BYTE LETTI %d\n", tmp->name, cont_lenght -1);
                        write(fd_c, &cont_lenght, sizeof(int));
                        write(fd_c, tmp->content, cont_lenght);
                        i++;
                    }
                    tmp = tmp->next;
                }
                pthread_mutex_unlock(&(fs->lock));
            }
            break;

        case 'w': //writeFile
            {
                fprintf(log_file, "\nRICHIESTA DI SCRITTURA\n");
                int pathname_lenght;
                READ_AND_BREAK(fd_c, &pathname_lenght, sizeof(int)); 
                char pathname[256];
                READ_AND_BREAK(fd_c, pathname, pathname_lenght); //leggo il nome del file
                fprintf(log_file, "\tNOME DEL FILE %s\n", pathname);
                File_node * tmp = file_get(pathname, fs);
                pthread_mutex_lock(&(fs->lock));
                int cont_lenght;
                READ_AND_BREAK(fd_c, &cont_lenght, sizeof(int));
                cont_lenght ++;
                char * cont = (char *) malloc(cont_lenght * sizeof(char));
                READ_AND_BREAK(fd_c, cont, cont_lenght);
                tmp->content = cont;
                tmp->cont_dim = strlen(tmp->content);
                tmp -> dim += tmp->cont_dim;
                fprintf(log_file, "\tBYTE SCRITTI : %zu\n", tmp->cont_dim);

                //AGGIORNO DIMENSIONE FILE SYSTEM
                fs->dim += tmp->cont_dim;
                pthread_mutex_unlock(&(fs->lock));
                //CONTROLLO CHE IL FILE NON SIA PIÙ GRANDE DELLA CAPACITÀ TOTALE DEL FILE SYSTEM
                if (tmp->dim > config->capacity) {
                    fprintf(log_file, "\tOPERAZIONE ANNULLATA , IL FILE È DI DIMENSIONI MAGGIORI DELLA CAPACITÀ DEL FILE SYSTEM\n");
                    fs->dim = fs->dim - (sizeof(File_node) + strlen(tmp->name));
                    remove_file(tmp->name, fs);
                    int check = 6;
                    write(fd_c, &check, sizeof(int));

                    return 0;
                }
                //RESTITUISCO GLI EVENTUALI FILE ESPULSI
                while ((fs->dim > config->capacity || fs->n_files > config->max_files) && keepRunning) {
                    fs->cache_calls ++;
                    fprintf(log_file, "\tESECUZIONE DELL' ALGORITMO DI RIMPIAZZAMENTO FIFO\n");

                    write(fd_c, &ONE, sizeof(int));
                    File_node * tmp = pop_file(fs);
                    int pathname_lenght = strlen(tmp->name) + 1;
                    int cont_lenght = strlen(tmp->content) + 1;
                    write(fd_c, &pathname_lenght, sizeof(int));
                    write(fd_c, tmp->name, pathname_lenght);
                    
                    write(fd_c, &cont_lenght, sizeof(int));
                    write(fd_c, tmp->content, cont_lenght);
                    fprintf(log_file, "\t\tNOME DEL FILE VITTIMA: %s , DIMENSIONI %zu\n", tmp->name, tmp->cont_dim);
                    free(tmp->content);
                    free(tmp->name);
                    free(tmp);
                }
                
                write(fd_c, &ZERO, sizeof(int));

                pthread_mutex_lock(&(fs->lock));
                //AGGIORNO IL RECORD DI FILE NEL FILE SYSTEM
                if(fs->n_files > fs->max_number_of_files_ever && fs->n_files <= config->max_files) {
                    fs->max_number_of_files_ever = fs->n_files;
                }
                //AGGIORNO LA DIMENSIONE MASSIMA RAGGIUNTA DAL FILE SYSTEM
                if (fs->dim > fs->max_dim_ever && fs->dim <= config->capacity) {
                    fs->max_dim_ever = fs->dim;
                }
                pthread_mutex_unlock(&(fs->lock));
            }
            break;

        case 'a': //appendToFile
            {
                fprintf(log_file, "\nRICHIESTA DI APPEND\n");
                int pathname_lenght, cont_lenght;
                char pathname[256];
                READ_AND_BREAK(fd_c, &pathname_lenght, sizeof(int));
                READ_AND_BREAK(fd_c, pathname, pathname_lenght);
                fprintf(log_file, "\tNOME DEL FILE : %s", pathname);
                READ_AND_BREAK(fd_c, &cont_lenght, sizeof(int));
                fprintf(log_file, "\tBYTE SCRITTI : %d\n", cont_lenght);
                char * cont = (char *) malloc(cont_lenght * sizeof(char));
                File_node * tmp = file_get(pathname, fs);
                READ_AND_BREAK(fd_c, cont, cont_lenght);
                
                //**OK, IL CONTENUTO ENTRA NEL FILE SYSTEM
                if (tmp->dim + cont_lenght <= config->capacity) {
                    pthread_mutex_lock(&(fs->lock));
                    tmp->content = realloc(tmp->content, (tmp->dim + cont_lenght) * sizeof(char));
                    strcat(tmp->content, cont);
                    free(cont);
                    //AGGIORNO DIMENSIONE DEL FILE E DEL FILE SYSTEM
                    fs->dim += cont_lenght;
                    tmp->dim += cont_lenght;
                    tmp->cont_dim += cont_lenght;
                    pthread_mutex_unlock(&(fs->lock));
                    fprintf(log_file, "\tDIMENSIONI DEL CONTENUTO TOTALE : %zu\n", tmp->cont_dim);


                    //RESTITUIRE GLI EVENTUALI FILE ESPULSI
                    while ((fs->dim > config->capacity || fs->n_files > config->max_files) && keepRunning) {
                        fprintf(log_file, "\tESECUZIONE DELL' ALGORITMO DI RIMPIAZZAMENTO FIFO\n");
                        fs->cache_calls ++;
                        write(fd_c, &ONE, sizeof(int));
                        File_node * tmp = pop_file(fs);
                        int pathname_lenght = strlen(tmp->name) + 1;
                        int cont_lenght = strlen(tmp->content) + 1;
                        write(fd_c, &pathname_lenght, sizeof(int));
                        write(fd_c, tmp->name, pathname_lenght);
                        
                        write(fd_c, &cont_lenght, sizeof(int));
                        write(fd_c, tmp->content, cont_lenght);
                        free(tmp->content);
                        free(tmp->name);
                        free(tmp);
                        fprintf(log_file, "\t\tNOME DEL FILE VITTIMA : %s , DIMENSIONI %zu\n", tmp->name, tmp->cont_dim);
                    }

                    write(fd_c, &ZERO, sizeof(int));
                }

                else {
                    fprintf(log_file, "\tOPERAZIONE FALLITA , IL CONTENUTO È MAGGIORE DELLA CAPACITÀ DEL FILE SYSTEM\n");
                    free(cont);
                    int check = 7;
                    write(fd_c, &check, sizeof(int));
                }

                pthread_mutex_lock(&(fs->lock));
                //AGGIORNO LA DIMENSIONE MASSIMA RAGGIUNTA DAL FILE SYSTEM
                if (fs->dim > fs->max_dim_ever && fs->dim <= config->capacity) {
                    fs->max_dim_ever = fs->dim;
                }
                pthread_mutex_unlock(&(fs->lock));
            }
            break;

        case 'l': //lockFile
            {
                fprintf(log_file, "\nRICHIESTA DI LOCK\n");
                int pathname_lenght;
                READ_AND_BREAK(fd_c, &pathname_lenght, sizeof(int));
                char pathname[256];
                READ_AND_BREAK(fd_c, pathname, pathname_lenght);
                fprintf(log_file, "\tNOME DEL FILE : %s\n", pathname);
                File_node * tmp = file_get(pathname, fs);
            
                if (!search_file(pathname, * fs)) {
                    fprintf(log_file, "\tFILE NON ESISTENTE\n");
                    int check = 2;
                    write(fd_c, &check, sizeof(int));
                    break;
                }
                    pthread_mutex_lock(&(fs->lock));
                
                if (tmp->locker != 0 && tmp->locker != fd_c) { //il file è già in modalità locked
                        pthread_cond_wait(&(fs->cond), &(fs->lock));
                }  
                if (keepRunning != 1) {
                    write(fd_c, &MENOUNO, sizeof(int));
                    pthread_mutex_unlock(&(fs->lock));
                    break;
                }             
                tmp->locker = fd_c;
                pthread_mutex_unlock(&(fs->lock));
                write(fd_c, &ZERO, sizeof(int));
                fprintf(log_file, "\tFILE LOCKATO CON SUCCESSO\n");                
            }
            break;

        case 'u': //unlockFile
            {
                fprintf(log_file, "\nRICHIESTA DI UNLOCK\n");
                int pathname_lenght;
                READ_AND_BREAK(fd_c, &pathname_lenght, sizeof(int));
                char pathname[256];
                READ_AND_BREAK(fd_c, pathname, pathname_lenght);
                fprintf(log_file, "\tNOME DEL FILE : %s\n", pathname);
                File_node * tmp;
                int check;
                if ((tmp = file_get(pathname, fs)) == NULL) {
                    check = 2;
                    fprintf(log_file, "\tFILE NON ESISTENTE\n");
                }
                else if (tmp->locker != fd_c && tmp->locker != 0) {
                    fprintf(log_file, "\tOPERAZIONE FALLITA , FILE LOCKATO DA UN ALTRO UTENTE\n");
                    check = 4; 
                }
                else if (tmp->locker == fd_c || tmp->locker == 0) {
                    pthread_mutex_lock(&(fs->lock));
                    pthread_cond_signal(&(fs->cond));
                    tmp->locker = 0;
                    pthread_mutex_unlock(&(fs->lock));
                    check = 0;
                    fprintf(log_file, "\tUNLOCK TERMINATO CON SUCCESSO\n");
                }
                
                write(fd_c, &check, sizeof(int));
            }
            
            break;

        case 'c': //closeFile
            {
                fprintf(log_file, "\nRICHIESTA DI CHIUSURA FILE\n");
                int pathname_lenght;
                READ_AND_BREAK(fd_c, &pathname_lenght, sizeof(int));
                char pathname[256];
                READ_AND_BREAK(fd_c, pathname, pathname_lenght);
                fprintf(log_file, "\tNOME DEL FILE : %s\n", pathname);
                int check;
                if (!search_file(pathname, *fs)) {//file non trovato
                    check = 2;
                    fprintf(log_file, "\tFILE NON ESISTENTE\n");
                }
                else {
                    File_node *tmp = file_get(pathname, fs);
                    pthread_mutex_lock(&(fs->lock));
                    tmp->file_users = remove_client(tmp->file_users, fd_c);
                    check = search_client(tmp->file_users, fd_c);
                    pthread_mutex_unlock(&(fs->lock));
                }
                write(fd_c, &check, sizeof(int));

                if (check) {
                    fprintf(log_file, "\tERRORE NELLA CHIUSURA DEL FILE\n");
                }
                else {
                    fprintf(log_file, "\tFILE CHIUSO CON SUCCESSO\n");
                }
            }
            break;

        case 'x': //removeFile
            {
                fprintf(log_file, "\nRICHIESTA DI RIMOZIONE\n");
                int pathname_lenght;
                READ_AND_BREAK(fd_c, &pathname_lenght, sizeof(int));
                char pathname[256];
                READ_AND_BREAK(fd_c, pathname, pathname_lenght);
                fprintf(log_file, "\tNOME DEL FILE : %s\n", pathname);
                int check;
                File_node *tmp;
                if (( tmp = file_get(pathname, fs)) != NULL) {
                    pthread_mutex_lock(&(fs->lock));
                     if (tmp->locker == 0) { //Il file non è in modalità locked
                        pthread_mutex_unlock(&(fs->lock));
                        fprintf(log_file, "\nOPERAZIONE FALLITA , FILE NON IN MODALITÀ LOCKED\n");
                        check = 5;
                    }
                    else if (tmp->locker != fd_c) {
                        pthread_mutex_unlock(&(fs->lock));
                        fprintf(log_file, "\tOPERAZIONE FALLITA , FILE LOCKATO DA UN ALTRO UTENTE\n");
                        check = 4;
                    }
                    else if (tmp->locker == fd_c) 
                        {
                        pthread_mutex_unlock(&(fs->lock));
                        check = remove_file(pathname, fs); 
                        if (check == 0)
                            fprintf(log_file, "\tFILE RIMOSSO CON SUCCESSO\n");
                        else    
                            fprintf(log_file, "\tOPERAZIONE FALLITA\n");
                    }
                }
                else {//file non trovato
                    check = 2;
                    fprintf(log_file, "\tFILE NON ESISTENTE\n");
                }
                write(fd_c, &check, sizeof(int));
            }
            break;

        case 'z' : //closeConnection
            {
                fprintf(log_file, "\nRICHIESTA DI CHIUSURA CONNESSIONE\n");
                reset_lock(fs, fd_c);   
                pthread_cond_signal(&(fs->cond));             
            }
            break;
    }
    return 0;
    }

void * thread_exec () {

    while (1) {
        pthread_mutex_lock(&tasks.lock);
       
        while (task_queue_lenght(&tasks) == 0 && !(keepRunning == -1 && task_queue_lenght(&tasks) == 0) && keepRunning) {
            pthread_cond_wait(&tasks.cond,&tasks.lock);
        }

        pthread_mutex_unlock(&tasks.lock);
        
        if (keepRunning == 0) {
            printf("THREAD TERMINATO\n");
            return NULL;
        }
        
        if (keepRunning == -1 && task_queue_lenght(&tasks) == 0) {
            printf("THREAD TERMINATO DOPO UN SIGHUP\n");
            return NULL;
        }

        int error = 0;
        socklen_t len = sizeof(error);  
        int retval;      

        if (task_queue_lenght(&tasks) != 0 ){

            int fd_c = pop_task(&tasks);
            retval = getsockopt(fd_c, SOL_SOCKET, SO_ERROR, &error, &len);
            if (error != 0 || retval != 0) {
                continue;
            }
            char request;

            //LEGGE LA PROSSIMA RICHIESTA DEL CLIENT, ALTRIMENTI RESETTA LE CORRISPONDENZE DEL FD_C DAL FILE SYSTEM
            if( ! read(fd_c, &request, sizeof(char))) { 
                reset_lock(fs, fd_c);
                continue;
            }

            task_exec(fd_c, request);

            /*CONTROLLO SE LA SOCKET DEL CLIENT È STATA UCCISA ALTRIMENTI CI SONO ALTRE RICHIESTE DA AGGIUNGERE IN CODA*/
            retval = getsockopt(fd_c, SOL_SOCKET, SO_ERROR, &error, &len);
            if (error == 0 && retval == 0){
                push_task(fd_c, &tasks);                
                pthread_cond_signal(&tasks.cond);
            }
        }
    }
    return NULL;
}


int main (int argc, char ** argv){

    struct sockaddr_un  server ;
    char * config_name;
    if (argc == 1) {
        config_name = "config.txt";
    }
    else {
        config_name = argv[1];
    }

    //INIZIALIZZAZIONE DELLA CONFIGURAZIONE TRAMITE IL FILE DI CONFIG
    config = NULL;
    config = (Config *) malloc(sizeof (Config));
    read_config_file(config_name);

    //CREAZIONE DEL FILE DI LOG
    if((log_file = fopen(config->log_file, "w")) == NULL) {
        printf("Errore nell' apertura del file di configurazione : %s\n", config->log_file);
        return -1;
    }


    fprintf(log_file,"*numero di thread : %d\n", config->thread_n);
    fprintf(log_file,"LETTURA CONFIG.TXT\n");
    fprintf(log_file,"*capacità di storage : %d\n", config->capacity);
    fprintf(log_file,"*nome del socketfile : %s\n", config->socketfile_name);
    fprintf(log_file,"*numero massimo di file memorizzabili : %d\n", config->max_files);
    fprintf(log_file,"*nome del file di log : %s\n", config->log_file);
    fprintf(log_file,"*numero massimo di connessioni pendenti : %d\n", config->max_pending_conn);


    //INIZIALIZZAZIONE DELLA CODA DI RICHIESTE    
    tasks.first = NULL;
    tasks.last = NULL;

    //INIZIALIZZAZIONE DEL FILE SYSTEM
    fs = (File_queue *) malloc(sizeof(File_queue));
    fs->dim = 0;
    fs->max_number_of_files_ever = 0;
    fs->first = NULL;
    fs->last = NULL;
    fs->n_files = 0;
    fs->max_dim_ever = 0;
    fs->cache_calls = 0;
    if (pthread_mutex_init(&(fs->lock),NULL) != 0){
        fprintf(log_file, "\nmutex_init FALLITA\n");
        return 1;
    }

    pthread_cond_init(&(fs->cond), NULL);

    //INIZIALIZZAZIONE DEL MULTITHREADING
    int thread_num = config->thread_n;

    if (pthread_mutex_init(&(tasks.lock),NULL) != 0){
        fprintf(log_file, "\nmutex_init FALLITA\n");
        return 1;
    }

    pthread_cond_init(&tasks.cond, NULL);

    pthread_t tid[thread_num];
    for (int i = 0; i < thread_num; i++) {
        if (pthread_create(&tid[i], NULL, &thread_exec, NULL) != 0) {
            perror("Fallita la creazione dei thread");
        }
        printf("thread %d\n",gettid());
    }
    

    //CREAZIONE SOCKET
    unlink(config->socketfile_name);

    fd_skt = socket(AF_UNIX,SOCK_STREAM, 0);
    memset((void *) &server, '0', sizeof(server));

    server.sun_family = AF_UNIX;
    strncpy(server.sun_path, config->socketfile_name, strlen(config->socketfile_name)+1);
    
    if(bind(fd_skt, (struct sockaddr *) &server, sizeof(server))<0){
        fprintf(log_file, "\nBind fallita\n");
        exit(EXIT_FAILURE);
    }
    
    //SEGNALI DI TERMINAZIONE
    struct sigaction a;
    a.sa_handler = intHandler;
    a.sa_flags = 0;
    sigemptyset( &a.sa_mask );
    sigaction( SIGINT, &a, NULL );
    sigaction( SIGQUIT, &a, NULL );
    sigaction( SIGHUP, &a, NULL );
    sigaction( SIGPIPE, &a, NULL );
    sigaction(SIGUSR1, &a, NULL );

    //CONNESSIONE E RICEZIONE RICHIESTE
    run_server(server);
    
    for (int i = 0; i < thread_num; i++) {
        if (pthread_join(tid[i], NULL) != 0) {
            fprintf(log_file, "pthread_join = NULL\n");
            perror("Fallita la join");
        }
        printf("join numero %d\n", i+1);
    }


    //SUNTO DELLE OPERAZIONI EFFETTUATE DURANTE L'ESECUZIONE
    printf("***SUNTO DELLE OPERAZIONI EFFETTUATE DURANTE L'ESECUZIONE***\n");
    printf("Numero di file massimo memorizzato nel server = %d\n",fs->max_number_of_files_ever);
    printf("Dimensione massima in Bytes raggiunta dal file storage = %zu\n", fs->max_dim_ever);
    printf("Numero di volte in cui l'algoritmo di rimpiazzamento della cache è stato eseguito per selezionare uno o più file vittima = %d\n", fs->cache_calls);
    stampa_tutto(*fs);
    fprintf(log_file, "\n***SUNTO DELLE OPERAZIONI EFFETTUATE DURANTE L'ESECUZIONE***\n\n");
    fprintf(log_file, "\t-Numero di file massimo memorizzato nel server = %d\n",fs->max_number_of_files_ever);
    fprintf(log_file, "\t-Dimensione massima in Bytes raggiunta dal file storage = %zu\n", fs->max_dim_ever);
    fprintf(log_file, "\t-Numero di volte in cui l'algoritmo di rimpiazzamento della cache è stato eseguito per selezionare uno o più file vittima = %d\n", fs->cache_calls);
    fprintf(log_file, "\n");
    stampa_tutto2(*fs, log_file);
    
    //LIBERO MEMORIA DEL FILE SYSTEM
    terminate_fs(fs, config, &tasks);
    
    //CHIUSURA FILE DI LOG
    fclose(log_file);

    return 0;

}