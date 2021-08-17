#include <pthread.h>
#include <stdio.h>
#include "queue.h"
#include <stdlib.h>
#include <time.h>

Task_queue queue; 


pthread_mutex_t mutexQueue;
pthread_cond_t condQueue;

//legge lo fd_c tramite la pop, cerca t
void executeTask (Task_queue *q) 
{
    //esempio di prova
    Task_node * t = q->first;
    int temp = t->fd_c;
    int res = pop_task(q) + 1;
    printf("operazione che somma 1 a %d = %d\n", temp, res);
}


void * startThread(void * args){
    while (1) {
        
        //pthread_mutex_lock(&mutexQueue);

         while (task_queue_lenght(&queue) == 0)
            pthread_cond_wait(&condQueue,NULL);
      
        //pthread_mutex_unlock(&mutexQueue);
        if (task_queue_lenght(&queue) != 0){
            executeTask(&queue);
        }
    }
}



int main (void) {

    int thread_num = 4;

    if (pthread_mutex_init(&(queue.lock),NULL) != 0){
        printf("\n mutex init failed\n");
        return 1;
    }

    pthread_mutex_init(&mutexQueue, NULL);
    pthread_cond_init(&condQueue, NULL);

    pthread_t tid[thread_num];
    for (int i = 0; i < thread_num; i++) {
        if (pthread_create(&tid[i], NULL, &startThread, NULL) != 0) {
            perror("Fallita la creazione dei thread");
        }
    }

    srand(time(NULL));
    for (int i = 0; i<10; i++){
        
        push_task(rand()%100, &queue);
        pthread_cond_signal(&condQueue);

    }


    for (int i = 0; i < thread_num; i++) {
        if (pthread_join(tid[i], NULL) != 0) {
            perror("Fallita la join");
        }
    }

    pthread_mutex_destroy(&mutexQueue);
    pthread_cond_destroy(&condQueue);

return 0;
}
