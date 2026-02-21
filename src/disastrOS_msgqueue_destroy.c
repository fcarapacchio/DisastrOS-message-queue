#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include "disastrOS.h"
#include "disastrOS_syscalls.h"
#include "disastrOS_message_queue.h"

void internal_mq_destroy() {
    int queue_id = running->syscall_args[0];

    if (queue_id <= 0) {
        running->syscall_retvalue = DSOS_EMQINVALID;
        return;
    }

    // find the resource by its id
    Resource* r = ResourceList_byId(&resources_list, queue_id);
    if (!r || r->type != RESOURCE_MESSAGE_QUEUE) {
        running->syscall_retvalue = DSOS_EMQINVALID;
        return;
    }

    MessageQueue* mq = (MessageQueue*) r;

    if (mq->status == MQ_CLOSED) {
        running->syscall_retvalue = DSOS_EMQCLOSED;
        return;
    }

    mq->status = MQ_CLOSING;

    // wake up all blocked receivers
    while (mq->waiting_receivers.first) {
        PCB* pcb = (PCB*) List_detach(&mq->waiting_receivers, mq->waiting_receivers.first);
        pcb->status = Ready;
        pcb->syscall_retvalue = DSOS_EMQNOTOPEN;
        List_insert(&ready_list, ready_list.last, (ListItem*)pcb);
    }

    // wake up all blocked senders
    while (mq->waiting_senders.first) {
        PCB* pcb = (PCB*) List_detach(&mq->waiting_senders, mq->waiting_senders.first);
        pcb->status = Ready;
        pcb->syscall_retvalue = DSOS_EMQNOTOPEN;
        List_insert(&ready_list, ready_list.last, (ListItem*)pcb);
    }

    // free the remaining messages
    while (mq->messages.first) {
        Message* msg = (Message*) List_detach(&mq->messages, mq->messages.first);
        Message_free(msg);
    }

    mq->status = MQ_CLOSED;

    // remove the message queue from the resource global list
    List_detach(&resources_list, (ListItem*) &mq->resource);

    // free the memory
    MessageQueue_free(mq);

    running->syscall_retvalue = queue_id;
}