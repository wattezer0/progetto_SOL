#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <limits.h>

#define QUEUE_EMPTY INT_MIN

typedef struct config
{
    int thread_n; //numero di thread workers
    int capacity; //capacità di storage 
    int max_files; //massimo numero di file nella memoria
    char * socketfile_name; //nome del socket file
    char * log_file; //nome del file di log
    int max_pending_conn; //numero massimo di connessioni pendenti
} Config;

typedef struct clients
{
    int fd_c;
    struct clients * next;
} Clients;

typedef struct file_node
{
    char * name; //nome del file
    Clients * file_users; //lista di client che hanno richiesto la openFile
    char * content; //contenuto del file 
    struct file_node * next;
    size_t dim; //dimensione che il file occupa
    size_t cont_dim; //dimensione allocata per il contenuto del file
    int locker; //fd_c del client che ha acquisito la lock, se il file è unlocked --> locker = 0
} File_node;

typedef struct file_queue
{
    File_node * first;
    File_node * last;
    int n_files; //numero di file
    int max_number_of_files_ever; //numero massimo di file mai memorizzati
    size_t max_dim_ever; //dimensione massima mai raggiunta
    size_t dim; //dimensione del File System
    int cache_calls; //numero di volte in cui è stato eseguito il rimpiazzamento
    pthread_mutex_t lock;
    pthread_cond_t cond;
} File_queue;

typedef struct task_node
{
    int fd_c; //file descriptort del client
    struct task_node * next;
} Task_node;

typedef struct task_queue
{
    Task_node * first;
    Task_node * last;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} Task_queue;

FILE * log_file;

//ESTRAE UN FILE DAL FILE SYSTEM
File_node * pop_file (File_queue * q) {
    
    pthread_mutex_lock(&(q->lock));
    
    if (q->first == NULL) {
        fprintf(log_file , "\npop_file FALLITA ! il file system è vuoto\n");
        pthread_mutex_unlock(&(q->lock));
        return NULL;
    }
    
    File_node * tmp = q->first;
    q->first = q->first->next;

    if (q->first == NULL)
        q->last = NULL;
    
   
    //AGGIORNARE LA DIMENSIONE DEL FILE SYSTEM E DECREMENTARE IL NUMERO DI FILE PRESENTI
    q->n_files --;
    q->dim = q->dim - tmp->dim;

    pthread_mutex_unlock(&(q->lock));
    return tmp;
}

//CREA UN NUOVO FILE 
File_node * push_new_file (char * pathname, int locker, File_queue * fq, Config * config)
{
   
    pthread_mutex_lock(&(fq->lock));
    File_node * new_elem = (File_node *) malloc(sizeof(File_node));

    new_elem->next = NULL;
    int name_lenght = strlen(pathname) + 1;
    new_elem->name = (char *) malloc (name_lenght * sizeof(char));
    strcpy(new_elem->name, pathname);
    new_elem->content = NULL;
    new_elem->file_users = NULL;
    new_elem->locker = locker; //se il file è rilasciato fd_c = 0
    new_elem->cont_dim = 0;
    new_elem->dim = sizeof(File_node) + name_lenght * sizeof(char);

    if (fq->last != NULL)
        fq->last->next = new_elem;
    
    fq->last = new_elem;

    if (fq->first == NULL)
        fq->first = new_elem;
    
    //aggiorno e controllo la dimensione del file system
    fq->dim += new_elem->dim;
    fq->n_files ++;

    pthread_mutex_unlock(&(fq->lock));
    return NULL;
    
}
//RESTITUISCE LA LUNGHEZZA DELLA LISTA DEI TASK
int task_queue_lenght (Task_queue * q) {
    
    Task_node * temp = q->first;
    int count = 0;
    while (temp != NULL) {
        count ++;
        temp = temp -> next;
    }
    return count;
}

//INSERISCE UNA RICHIESTA NELLA CODA
void push_task (int fd_c, Task_queue * queue)
{
   
    pthread_mutex_lock(&(queue->lock));

    Task_node * new_elem = (Task_node *) malloc(sizeof(Task_node));

    new_elem->next = NULL;
    new_elem->fd_c = fd_c;
    
    if (queue->last != NULL)
        queue->last->next = new_elem;
    
    queue->last = new_elem;

    if (queue->first == NULL)
        queue->first = new_elem;
      
    fprintf(log_file, "NUMERO DI CONNESSIONI ATTUALI : %d\n", task_queue_lenght(queue));
    pthread_mutex_unlock(&(queue->lock));
    
}

//ESTRAE UNA RICHIESTA DALLA CODA
int pop_task (Task_queue * q){
    pthread_mutex_lock(&(q->lock));
    if (q->first == NULL) {
        //printf("La coda è vuota!\n");
        pthread_mutex_unlock(&(q->lock));
        return QUEUE_EMPTY;
    }
    
    Task_node * tmp = q->first;

    int res = tmp->fd_c;

    q->first = q->first->next;
    if (q->first == NULL)
        q->last = NULL;

    free(tmp);

    pthread_mutex_unlock(&(q->lock));
    return res;
}


//STAMPA LA LISTA DEI CLIENT NELLA CODA DI RICHIESTE
void print_clients (Task_queue q) {
    Task_node * tmp = q.first;
    while (tmp != NULL) {
        printf("fd_c = %d\n", tmp->fd_c);
        tmp = tmp->next;
    } 
}

//cerca un file nel file system e controlla che il locker sia fd_c
int search_file (char * name, File_queue fs) {
    File_node * tmp = fs.first;
    while (tmp != NULL) {
        if (strcmp(name, tmp->name) == 0 ) {
                return 1; //file trovato e disponibile
        }
        else tmp = tmp -> next;
    }
    return 0; //file non trovato
}

//RESTITUISCE IL FILE NAME , ALTRIMENTI NULL
File_node * file_get (char * name, File_queue * fs) {
    pthread_mutex_lock(&(fs->lock));
    File_node * tmp = fs->first;
    while (tmp != NULL) {
        if (strcmp(name, tmp->name) == 0) {
            pthread_mutex_unlock(&(fs->lock));
            return tmp;
        }
        else { 
            tmp = tmp->next;
        }
    }
    pthread_mutex_unlock(&(fs->lock));
    return NULL;
}

//setta il locker del file, ritorna 1 se ha successo, 0 altrimenti
int set_locker (char * name, int fd_c,  File_queue * fs) {
    File_node * tmp = file_get (name, fs);
    if (tmp && (tmp->locker == 0 || tmp->locker == fd_c)) {
        tmp->locker = fd_c;
        return 1;
    }
    else 
        return 0;
}

//STAMPA SU STDOUT LA LISTA DEI FILE NEL FILE SYSTEM
void stampa_tutto (File_queue fs) {
    File_node * tmp = fs.first;
    printf("\n");
    while(tmp != NULL)
        {
            printf("Nome del file : %s\n",tmp->name);
            //printf("Contenuto del file : %s\n", tmp->content);
            tmp = tmp->next;
        }
    printf("\n");
}

//STAMPA SUL FILE DI LOG LA LISTA DEI FILE NEL FILE SYSTEM
void stampa_tutto2 (File_queue fs, FILE * log_file) {
    File_node * tmp = fs.first;
    fprintf(log_file, "\t-LISTA DEI FILE NEL FILE SYSTEM :\n");
    while(tmp != NULL)
        {
            fprintf(log_file, "\t\tNome del file : %s\n",tmp->name);
            // fprintf(log_file, "\t\tContenuto del file : %s\n", tmp->content);
            tmp = tmp->next;
        }
}

//STAMPA IL NUMERO TOTALE DI FILE NEL FILE SYSTEM
void fs_file_count(File_queue fs) {
    File_node * tmp = fs.first;
    printf("\n");
    int count = 0;
    while(tmp != NULL)
        {
            count ++;
            tmp = tmp->next;
        }
    //printf("Ci sono %d File nel file system\n", count);
}

//RESTITUISCE IL NUMERO DI FILE NEL FILE SYSTEM LEGGIBILI DA FD_C
int unlocked_files_count (int locker, File_queue fs) {
    File_node * tmp = fs.first;
    int count = 0;
    while(tmp != NULL && tmp->content != NULL) {
        if ( tmp->locker == 0 || tmp->locker == locker)
            count++;
        tmp = tmp->next;
    }
    return count;
}

//RIMUOVE UN FILE DAL FILE SYSTEM 
int remove_file (char * pathname, File_queue * fs) {
    pthread_mutex_lock(&(fs->lock));
    File_node * tmp = fs ->first;
    while (strcmp(tmp->name, pathname) != 0) {
        tmp = tmp -> next;
    }
    if (tmp == NULL) {
        pthread_mutex_unlock(&(fs->lock));
        return 2;
    }
    if (tmp == fs ->first && tmp == fs ->last) { //è l'unico file
        fs -> first = NULL;
        fs -> last = NULL;
    }
    else if (tmp == fs ->first) {     //è il file di testa
        fs -> first = tmp -> next;

    }
    else if (tmp == fs ->last) {     //è l' ultimo file 
        File_node * prec = fs ->first;
        while (prec -> next != tmp) {
            prec = prec -> next;
        }
        prec -> next = NULL;
        fs -> last = prec;  
    }
    else { //è un file di mezzo della coda
        File_node * prec = fs ->first;
        while (prec -> next != tmp) {
            prec = prec -> next;
        }
        prec -> next = tmp -> next;
    }
    //aggiornamento dimensione file system
    int dim = tmp->dim - tmp -> cont_dim > 0 ? tmp -> dim : tmp -> cont_dim;
    fs ->dim -= dim;
    fs ->n_files --;
    
    //eliminazione contenuto di tmp
    free(tmp->name);
    free(tmp->content);
    free(tmp);

    pthread_mutex_unlock(&(fs->lock));
    return 0;
}

//CERCA UN CLIENT NELLA LISTA DI CLIENT NEL FILE
int search_client(Clients * active_clients, int fd_c){
    Clients * tmp = active_clients;

    while(tmp != NULL) {
        if(tmp->fd_c == fd_c) {
            return 1;
        }
        tmp = tmp->next;
    }
    return 0;
}

//AGGIUNGE UN CLIENT ALLA LISTA DI CLIENT NEL FILE
Clients * add_client (Clients * active_clients, int fd_c) {
    if( !search_client(active_clients, fd_c)) { //elemento non presente nella lista
        Clients * new_elem = (Clients *) malloc(sizeof(Clients));
        new_elem->next = active_clients;
        new_elem->fd_c = fd_c;
        return new_elem; 
    }
    return active_clients;
}

//RIMUOVE UN CLIENT DALLA LISTA NEL FILE
Clients * remove_client (Clients * active_clients, int fd_c) {
    
    if (active_clients == NULL) {
        return NULL;
    }
    
    Clients *tmp = active_clients;
    Clients *prec = NULL;
    while (tmp != NULL && tmp->fd_c != fd_c) {
        prec = tmp;
        tmp = tmp->next;
    }
    if (tmp == NULL) { //fd_c non trovato
        fprintf(log_file, "Client non trovato\n");
        return active_clients;
    }
    if (prec==NULL) { //devo eliminare il primo elemento
        active_clients = tmp->next;
    }
    else {
        prec->next = tmp->next;
    }
    free (tmp);

    return active_clients;
}

void stampa_clienti_del_file (Clients * active_clients) {
    Clients * tmp = active_clients;
    if (tmp == NULL) {
        //printf("Non ci sono client attivi\n");
        return;
    }
    while (tmp != NULL)  {
        //printf("Client attivo %d\n", tmp->fd_c);
        tmp = tmp->next;
    }
}

//LIBERA TUTTA LO HEAP UTILIZZATO
void terminate_fs (File_queue * fs, Config * config, Task_queue * tasks) { 
    
    //LIBERA LA MEMORIA DEL FILE SYSTEM
    pthread_mutex_lock(&(fs->lock));
    File_node * tmp = fs->first;
    File_node * prec = NULL;
    while (tmp != NULL) { 
        prec = tmp;
        tmp = tmp->next;
        free(prec->name);
        free(prec->content);
        Clients * victim = prec->file_users;
        while(prec->file_users != NULL){
            victim = prec->file_users;
            prec->file_users = prec->file_users->next;
            free(victim);
        }
        free(prec->file_users);
        free(prec);
    }
    pthread_mutex_unlock(&(fs->lock));
    pthread_mutex_destroy(&(fs->lock));
    pthread_cond_destroy(&(fs->cond));
    free(fs);

    //LIBERA LA MEMORIA DI CONFIG
    free(config->log_file);
    free(config->socketfile_name);
    free(config);

    //LIBERA MEMORIA DI TASKS
    pthread_mutex_lock(&(tasks->lock));
    Task_node * t = tasks->first;
    Task_node * t2 = NULL;
    while (t != NULL) {
        t2 = t;
        t = t->next;
        free(t2);
    }
    pthread_mutex_unlock(&(tasks->lock));
    pthread_mutex_destroy(&(tasks->lock));
    pthread_cond_destroy(&(tasks->cond));
}

//RESETTA TUTTE LE CORRISPONDENZE DI FD_C NEL FILE SYSTEM
void reset_lock (File_queue * fs, int fd_c) {
    File_node * tmp = fs->first;
    while (tmp != NULL) {
        if (tmp->locker == fd_c) {
            tmp->locker = 0;
        }
        tmp->file_users = remove_client(tmp->file_users, fd_c);
        tmp = tmp->next;
    }
}
