
#ifndef QUEUE_H
#define QUEUE_H

#include "common.h"

#define MAX_QUEUE_SIZE 10

//
// wrapper of the pcb queue
//
struct qitem_t
{
	struct pcb_t *data;
	struct qitem_t *next;
};

struct queue_t
{
	//struct pcb_t * proc[MAX_QUEUE_SIZE];
	struct qitem_t *head;
	struct qitem_t *tail;
    int slot_left; // remaining time_slot
	int size;
    // int done_time_slot;
};

void enqueue(struct queue_t * q, struct pcb_t * proc);

struct pcb_t * dequeue(struct queue_t * q);

int empty(struct queue_t * q);

#endif

