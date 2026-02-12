#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include "disastrOS.h"
#include "disastrOS_syscalls.h"
#include "disastrOS_message_queue.h" 

void internal_mq_send() {
    int queue_id = running->syscall_args[0];
    Message* msg = (Message*) running->syscall_args[1];

    // find the queue
    Resource* res = ResourceList_byId(&resources_list, queue_id);
    if (!res || res->type != RESOURCE_MESSAGE_QUEUE) {
        running->syscall_retvalue = DSOS_EMQINVALID;
        return;
    }

    MessageQueue* mq = (MessageQueue*) res;

    // if queue is full, block the process
    if (mq->current_messages >= mq->max_messages) {
        running->status = Waiting;
        List_insert(&mq->waiting_senders,mq->waiting_senders.last, (ListItem*) running);
        internal_schedule();
        return;
    }

    // insert the message
    List_insert(&mq->messages, mq->messages.last, (ListItem*)msg);
    mq->current_messages++;

    // wake up a blocked receiver
    if (mq->waiting_receivers.first) {
        PCB* receiver = (PCB*) List_popFront(&mq->waiting_receivers);
        receiver->status = Ready;
        List_insert(&ready_list, ready_list.last, (ListItem*) receiver);
    }

    running->syscall_retvalue = 0; // success
}