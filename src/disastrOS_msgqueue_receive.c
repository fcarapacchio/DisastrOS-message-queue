#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "disastrOS.h"
#include "disastrOS_syscalls.h"
#include "disastrOS_message_queue.h"

void internal_mq_receive() {
    int queue_id = running->syscall_args[0];
    void* user_buffer = (void*) running->syscall_args[1];
    int buffer_size = running->syscall_args[2];

    if (buffer_size <= 0) {
    running->syscall_retvalue = DSOS_EBUFFER;
    return;
    }

    Resource* res = ResourceList_byId(&resources_list, queue_id);
    if (!res || res->type != RESOURCE_MESSAGE_QUEUE) {
        running->syscall_retvalue = DSOS_EMQINVALID;
        return;
    }

    MessageQueue* mq = (MessageQueue*) res;

    // if queue is empty, block the current process
    while (!mq->messages.first) {
        if (mq->status != MQ_OPEN) {
            running->syscall_retvalue = DSOS_EMQNOTOPEN;
            return;
    }

    running->status = Waiting;
    List_insert(&mq->waiting_receivers, mq->waiting_receivers.last, (ListItem*) running);
    internal_schedule();
}

    // take the first message
    Message* msg = (Message*)List_detach(&mq->messages, mq->messages.first);
    mq->current_messages--;

    if (buffer_size < msg->size) {
    Message_free(msg);
    running->syscall_retvalue = DSOS_EBUFFER; 
    return; 
  }

  memcpy(user_buffer, msg->data, msg->size);

  int received_size = msg->size;

  Message_free(msg);

    // if a sender is blocked, wake him up
    if (mq->waiting_senders.first) {
        PCB* sender = (PCB*) List_detach(&mq->waiting_senders, mq->waiting_senders.first);
        sender->status = Ready;
        List_insert(&ready_list, ready_list.last, (ListItem*) sender);
    }

    running->syscall_retvalue = received_size;
}