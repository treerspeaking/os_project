#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
        if (q == NULL) return 1;
	return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {
    /* TODO: put a new process to queue [q] */
    //
    // có thể implement thêm mutex chỉ lock khi enqueue vào prio của queue nhất định
    //
    struct qitem_t * new_item = (struct qitem_t *)malloc(sizeof(struct qitem_t));
	new_item->data=proc;
	if(empty(q)){
		q->head = new_item;
		q->tail = new_item;
		q->tail->next = NULL;
        q->size = 1;
	}
	else{
		q->tail->next = new_item;
		q->tail = q->tail->next;
		q->tail->next = NULL;
        q->size++;
	}
}

struct pcb_t * dequeue(struct queue_t * q) {
    /* TODO: return a pcb whose prioprity is the highest
        * in the queue [q] and remember to remove it from q
        * */
    //
    // có thể implement thêm mutex chỉ lock khi dequeue vào prio của queue nhất định
    //
    struct pcb_t * proc = NULL;
    if(empty(q)){
		return proc;
	}
	// set proc to q->head->data to return
	proc = q->head->data;
	struct qitem_t * del_item = q->head;
	q->head = q->head->next;
	if(empty(q)){
		q->head = NULL;
		q->tail = NULL;
	}
    q->size-=1;
	// delete the del_item
	free(del_item);
	return proc;
}

