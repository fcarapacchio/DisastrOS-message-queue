#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include "disastrOS.h"
#include "disastrOS_syscalls.h"
#include "disastrOS_message_queue.h"

Message* internal_mq_receive() {
    int queue_id = running->syscall_args[0];

    Resource* res = ResourceList_byId(&resources_list, queue_id);
    if (!res || res->type != RESOURCE_MESSAGE_QUEUE) {
        running->syscall_retvalue = DSOS_EMQINVALID;
        return 0;
    }

    MessageQueue* mq = (MessageQueue*) res;

    // if queue is empty, block the current process
    if (!mq->messages.first) {
        if (mq->status != MQ_OPEN) return 0; 
      
        running->status = Waiting;
        List_insert(&mq->waiting_receivers, mq->waiting_receivers.last, (ListItem*) running);
        internal_schedule();
        return 0;
    }

    // take the first message
    Message* msg = (Message*)List_detach(&mq->messages, mq->messages.first);
    mq->current_messages--;

    // if a sender is blocked, wake him up
    if (mq->waiting_senders.first) {
        PCB* sender = (PCB*) List_detach(&mq->waiting_senders, mq->waiting_senders.first);
        sender->status = Ready;
        List_insert(&ready_list, ready_list.last, (ListItem*) sender);
    }

    running->syscall_retvalue = 0; // success
    return msg;
}