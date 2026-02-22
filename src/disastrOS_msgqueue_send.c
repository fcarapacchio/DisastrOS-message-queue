#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "disastrOS.h"
#include "disastrOS_syscalls.h"
#include "disastrOS_message_queue.h" 

void internal_mq_send() {
    PCB* caller = running;
    int queue_id = caller->syscall_args[0];
    void* user_buffer = (void*) caller->syscall_args[1];
    int size = caller->syscall_args[2];


    if (queue_id <= 0) {
        caller->syscall_retvalue = DSOS_EMQINVALID;
        return;
    }

    if (size <= 0 || size > MESSAGE_MAX_SIZE) {
        caller->syscall_retvalue = DSOS_EBUFFER;
        return;
    }

    // find the queue
    Resource* res = ResourceList_byId(&resources_list, queue_id);
    if (!res || res->type != RESOURCE_MESSAGE_QUEUE) {
        caller->syscall_retvalue = DSOS_EMQINVALID;
        return;
    }

    MessageQueue* mq = (MessageQueue*) res;

    // if queue is full, block the process
    if (mq->current_messages >= mq->max_messages) {
        printf("[mq_send] queue %d FULL (%d/%d), pid=%d enters waiting_senders\n",
               mq->queue_id,
               mq->current_messages,
               mq->max_messages,
               caller->pid);
        if (mq->status != MQ_OPEN) {
            caller->syscall_retvalue = DSOS_EMQNOTOPEN;
            return;
        }
        caller->status = Waiting;
        List_insert(&mq->waiting_senders,mq->waiting_senders.last, (ListItem*) caller);
        printf("[mq_send] pid=%d inserted into waiting_senders on queue %d\n", caller->pid, mq->queue_id);
        MessageQueue_print(mq->queue_id);
        internal_schedule();

        // no process was schedulable: avoid spinning forever in this syscall
        if (running == caller && caller->status == Waiting) {
            List_detach(&mq->waiting_senders, (ListItem*) caller);
            caller->status = Running;
            caller->syscall_retvalue = DSOS_EMQFULL;
            return;
        }
        
        // If scheduler switched to a different process, stop here.
        if (running != caller)
          return;

        // queue still full for caller after resume
        if (mq->current_messages >= mq->max_messages) {
          return;
        }
    }

    Message* msg = Message_alloc(size);
    if(!msg){
        caller->syscall_retvalue = DSOS_EBUFFER;
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
               caller->pid);
    }

    MessageQueue_print(mq->queue_id);

    // checking waiting receivers list, in case it's not empty we wake up one process
    if (mq->waiting_receivers.first) {
        PCB* receiver = (PCB*) List_detach(&mq->waiting_receivers, mq->waiting_receivers.first);
        void* receiver_buffer = (void*) receiver->syscall_args[1];
        int receiver_buffer_size = receiver->syscall_args[2];

        msg = (Message*)List_detach(&mq->messages, mq->messages.first);
        mq->current_messages--;

        if (receiver_buffer_size < msg->size) {
            receiver->syscall_retvalue = DSOS_EBUFFER;
        } else {
            memcpy(receiver_buffer, msg->data, msg->size);
            receiver->syscall_retvalue = msg->size;
        }

        Message_free(msg);

        receiver->status = Ready;
        List_insert(&ready_list, ready_list.last, (ListItem*) receiver);

        caller->syscall_retvalue = 0;
        return;
    }

    caller->syscall_retvalue = 0; // success
}