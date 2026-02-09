#include "disastrOS_message_queue.h"
#include "disastrOS.h"
#include "disastrOS_syscalls.h"

#include "linked_list.h"
#include "disastrOS_message.h"
#include "pool_allocator.h"

#define MESSAGE_QUEUE_SIZE sizeof(MessageQueue)
#define MESSAGE_QUEUE_BUFFER_SIZE MESSAGE_QUEUE_SIZE*MAX_MESSAGE_QUEUES
static char _message_queue_buffer[MESSAGE_QUEUE_BUFFER_SIZE];
/*
 * message queue pool allocator
 */
static PoolAllocator message_queue_allocator;

/*
 * message queue initialization
 */
void message_queue_init() {
  PoolAllocatorResult result = PoolAllocator_init(&message_queue_allocator,
                               MESSAGE_QUEUE_SIZE, 
                               MAX_MESSAGE_QUEUES,
                               _message_queue_buffer,
                               MESSAGE_QUEUE_BUFFER_SIZE);
    assert(!result);  
}

MessageQueue* MessageQueue_create(int max_messages) {
  if (max_messages<=0 || max_messages>MAX_MESSAGES_PER_QUEUE){
    return 0;
  }

  MessageQueue* mq = (MessageQueue*)PoolAllocator_getBlock(&message_queue_allocator);

  if (!mq)
    return 0;

  List_init(&mq->messages);
  List_init(&mq->waiting_receivers);
  List_init(&mq->waiting_senders);

  mq->max_messages = max_messages;
  mq->current_messages = 0;

  return mq;
}

void MessageQueue_send(MessageQueue* mq, Message* msg) {
  if (!mq || !msg)
    return;

    while (mq->current_messages >= mq->max_messages) {
      PCB* current = running;
      current->status = Waiting;
      List_insert(&mq->waiting_senders, mq->waiting_senders.last, (ListItem*)current);
      disastrOS_schedule();
}

  List_insert(&mq->messages, mq->messages.last, (ListItem*)msg);

    mq->current_messages++;

  /*
   * if someone is waiting, wake him up
   */
  if (mq->waiting_receivers.first) {
    PCB* pcb = (PCB*)List_detach(&mq->waiting_receivers, mq->waiting_receivers.first);
    pcb->status = Ready;
    List_insert(&ready_list, ready_list.last, (ListItem*) pcb);
  }
}


Message* MessageQueue_receive(MessageQueue* mq) {
  if (!mq)
    return 0;

  /*
   * queue is empty, block the current process
   */
  while (!mq->messages.first) {
    PCB* current = running;
    current->status = Waiting;
    List_insert(&mq->waiting_receivers, mq->waiting_receivers.last, (ListItem*) current);
    disastrOS_schedule();
  }

  /*
   * there is a message
   */
  Message* msg = (Message*)List_detach(&mq->messages, mq->messages.first);
mq->current_messages--;

if (mq->waiting_senders.first){
  PCB* pcb = (PCB*) List_detach(&mq->waiting_senders, mq->waiting_senders.first);
  pcb->status = Ready;
  List_insert(&ready_list, ready_list.last, (ListItem*) pcb);
}

  return msg;
}

void MessageQueue_destroy(MessageQueue* mq) {
  if (!mq)
    return;

  /*
   * free the remaining messages
   */
  ListItem* it = mq->messages.first;
  while (it) {
    ListItem* next = it->next;
    Message_free((Message*)it);
    it = next;
  }

  PoolAllocator_releaseBlock(&message_queue_allocator, mq);
}

// prints the message queue status, focusing on messages and waiting sending/receiving processes
void MessageQueue_print_status(MessageQueue* mq){
  printf("MQ id=%d, max=%d, current=%d, status=%d\n",
           mq->queue_id, mq->max_messages, mq->current_messages, mq->status);
    
    printf("Messages:\n");
    ListItem* it = mq->messages.first;
    while(it) {
        Message* msg = (Message*)it;
        printf("  size=%d, data[0]=%c\n", msg->size, msg->data[0]);
        it = it->next;
    }

    printf("Waiting receivers:\n");
    it = mq->waiting_receivers.first;
    while(it) {
        PCB* pcb = (PCB*)it;
        printf("  PID=%d\n", pcb->pid);
        it = it->next;
    }

    printf("Waiting senders:\n");
    it = mq->waiting_senders.first;
    while(it) {
        PCB* pcb = (PCB*)it;
        printf("  PID=%d\n", pcb->pid);
        it = it->next;
    }
}

