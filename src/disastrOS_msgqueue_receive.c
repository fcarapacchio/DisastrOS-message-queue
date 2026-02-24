#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "disastrOS.h"
#include "disastrOS_syscalls.h"
#include "disastrOS_message_queue.h"

void internal_mq_receive() {
    PCB* caller = running;
    int queue_id = caller->syscall_args[0];
    void* user_buffer = (void*) caller->syscall_args[1];
    int buffer_size = caller->syscall_args[2];

    if (queue_id <= 0) {
      running->syscall_retvalue = DSOS_EMQINVALID;
      return;
    }

    if (buffer_size <= 0) {
      caller->syscall_retvalue = DSOS_EBUFFER;
      return;
    }

    Resource* res = ResourceList_byId(&resources_list, queue_id);
    if (!res || res->type != RESOURCE_MESSAGE_QUEUE) {
        caller->syscall_retvalue = DSOS_EMQINVALID;
        return;
    }

    MessageQueue* mq = (MessageQueue*) res;

    printf("[mq_receive] Trying to receive a message from queue %d (receive also wakes up a waiting sender)\n", mq->queue_id);

    // if queue is empty, block the current process
    if (!mq->messages.first) {
        if (mq->status != MQ_OPEN) {
            caller->syscall_retvalue = DSOS_EMQNOTOPEN;
            return;
        }

        caller->status = Waiting;
        List_insert(&mq->waiting_receivers, mq->waiting_receivers.last, (ListItem*) caller);
        printf("[mq_receive] pid=%d inserted into waiting_receivers on queue %d\n", caller->pid, mq->queue_id);
        MessageQueue_print(mq->queue_id);
        internal_schedule();

        // no process was schedulable: avoid spinning forever in this syscall
        if (running == caller && caller->status == Waiting) {
        List_detach(&mq->waiting_receivers, (ListItem*) caller);
        caller->status = Running;
        caller->syscall_retvalue = DSOS_EMQEMPTY;
        return;
      }
      
      // If scheduler switched to a different process, stop here.
      if (running != caller) {
        return;
      }

      // queue is still empty for caller after resume
      if (!mq->messages.first) {
        return;
      }
}
    
    // take the first message
    Message* msg = (Message*)List_detach(&mq->messages, mq->messages.first);
    mq->current_messages--;

    if (buffer_size < msg->size) {
      Message_free(msg);
      caller->syscall_retvalue = DSOS_EBUFFER; 
      return; 
    }

    memcpy(user_buffer, msg->data, msg->size);
    int received_size = msg->size;
    Message_free(msg);

    MessageQueue_print(mq->queue_id);

    // If a sender is blocked because queue was full, complete one pending send here.
    if (mq->waiting_senders.first) {
        PCB* sender = (PCB*) List_detach(&mq->waiting_senders, mq->waiting_senders.first);
        int sender_size = sender->syscall_args[2];  // sender->syscall_args belong to THAT sender PCB (saved at mq_send entry), not to the current receiver
        void* sender_buffer = (void*) sender->syscall_args[1];

        if (sender_size <= 0 || sender_size > MESSAGE_MAX_SIZE) {
          sender->syscall_retvalue = DSOS_EBUFFER;
        } else {
          Message* sender_msg = Message_alloc(sender_size);
          if (!sender_msg) {
            sender->syscall_retvalue = DSOS_EBUFFER;
          } else {
            memcpy(sender_msg->data, sender_buffer, sender_size);
            List_insert(&mq->messages, mq->messages.last, (ListItem*) sender_msg);
            mq->current_messages++;
            sender->syscall_retvalue = 0;
          }
        }

        sender->status = Ready;
        List_insert(&ready_list, ready_list.last, (ListItem*) sender);
    }

    caller->syscall_retvalue = received_size;
}