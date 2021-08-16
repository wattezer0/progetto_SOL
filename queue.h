#include <stdlib.h>
#include <string.h>
#include<pthread.h>

#define QUEUE_EMPTY INT_MIN


typedef struct file_node
{
    //void * element;
    //struct node * next;
} File_node;

typedef struct file_queue
{
    File_node * first;
    File_node * last;
    int maxSize; //massimo numero di file accettati dal server
} File_queue;

typedef struct task_node
{
    int fd_c;
    struct task_node * next;
} Task_node;

typedef struct task_queue
{
    Task_node * first;
    Task_node * last;
    pthread_mutex_t lock;
} Task_queue;

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
         
    
    pthread_mutex_unlock(&(queue->lock));
    
}

int pop_task (Task_queue * q){

    if (pthread_mutex_init(&q->lock,NULL) != 0){
        printf("\n mutex init failed\n");
        return 1;
    }
    pthread_mutex_lock(&(q->lock));
    if (q->first == NULL) return QUEUE_EMPTY;
    
    Task_node * tmp = q->first;

    int res = tmp->fd_c;

    q->first = q->first->next;
    if (q->first == NULL)
        q->last = NULL;

    free(tmp);

    pthread_mutex_unlock(&(q->lock));
    return res;
}

int task_queue_lenght (Task_queue * q) {
    
    Task_node * temp = q->first;
    int count = 0;
    while (temp != NULL) {
        count ++;
        temp = temp -> next;
    }
    return count;
}