#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "disastrOS.h"
#include "disastrOS_syscalls.h"
#include "disastrOS_message_queue.h" 

void internal_mq_send() {
    int queue_id = running->syscall_args[0];
    void* user_buffer = (void*) running->syscall_args[1];
    int size = running->syscall_args[2];

    if (size <= 0) {
    running->syscall_retvalue = DSOS_EBUFFER;
    return;
  }

    // find the queue
    Resource* res = ResourceList_byId(&resources_list, queue_id);
    if (!res || res->type != RESOURCE_MESSAGE_QUEUE) {
        running->syscall_retvalue = DSOS_EMQINVALID;
        return;
    }

    MessageQueue* mq = (MessageQueue*) res;

    // if queue is full, block the process
    while(mq->current_messages >= mq->max_messages) {
        printf("[mq_send] queue %d FULL (%d/%d), pid=%d enters waiting_senders\n",
               mq->queue_id,
               mq->current_messages,
               mq->max_messages,
               running->pid);
        if (mq->status != MQ_OPEN) {
            running->syscall_retvalue = DSOS_EMQNOTOPEN;
            return;
    }
        running->status = Waiting;
        List_insert(&mq->waiting_senders,mq->waiting_senders.last, (ListItem*) running);
        MessageQueue_print_status(mq->queue_id);
        internal_schedule();
    }

    Message* msg = Message_alloc(size);
    if(!msg){
        running->syscall_retvalue = DSOS_EBUFFER;
        return;
    }

    memcpy(msg->data, user_buffer, size);

    // insert the message
    List_insert(&mq->messages, mq->messages.last, (ListItem*)msg);
    mq->current_messages++;
    if (mq->current_messages == mq->max_messages) {
        printf("[mq_send] queue %d reached FULL state (%d/%d) after pid=%d send\n",
               mq->queue_id,
               mq->current_messages,
               mq->max_messages,
               running->pid);
    }

    // wake up a blocked receiver
    if (mq->waiting_receivers.first) {
        PCB* pcb = (PCB*) List_detach(&mq->waiting_receivers, mq->waiting_receivers.first);
        pcb->status = Ready;
        List_insert(&ready_list, ready_list.last, (ListItem*) pcb);
    }

    running->syscall_retvalue = 0; // success
}