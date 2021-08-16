#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include "queue.h"

int estrai (Task_queue * q){
    Task_node * tmp=q->first;
    int res = tmp-> fd_c;
    q->first=q->first->next;
    if(q->first==NULL)
        q->last=NULL;
    free(tmp);
    return res;
}

int main (){
    Task_queue  * q = (Task_queue *) malloc(sizeof(Task_queue));
    Task_queue q1;
    q->first=NULL;
    q->last=NULL;
    q1.first=NULL;
    q1.last=NULL;

    push_task(112,&q1);
    
    push_task(22,&q1);

    push_task(2323,&q1);

    push_task(5325,&q1);

    push_task(5346,&q1);

    push_task(4366,&q1);

    push_task(634,&q1);

    printf("**Lunghezza coda : %d\n", task_queue_lenght(&q1));
    printf("%d\n", pop_task(&q1));

    printf("%d\n", pop_task(&q1));

    printf("%d\n", pop_task(&q1));

    printf("%d\n", pop_task(&q1));

    printf("%d\n", pop_task(&q1));

      printf("%d\n", pop_task(&q1));

    printf("%d\n", pop_task(&q1));

    printf("%d\n", pop_task(&q1));

    printf("%d\n", pop_task(&q1));


    return 0;
}