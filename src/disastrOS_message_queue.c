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
  
  // Resource part
  mq->resource.id = generate_unique_mq_id();
  mq->resource.type = RESOURCE_MESSAGE_QUEUE;
  mq->resource.ref_count = 0;
  mq->resource.resource_data = mq;
  List_init(&mq->resource.descriptors_ptrs);
  
  // Message Queue part
  List_init(&mq->messages);
  List_init(&mq->waiting_receivers);
  List_init(&mq->waiting_senders);

  mq->max_messages = max_messages;
  mq->current_messages = 0;
  mq->queue_id = mq->resource.id;
  mq->status = MQ_OPEN;
  
  // insert the message queue in the resource global list
  List_insert(&resources_list, resources_list.last, (ListItem*) &mq->resource);

  return mq;
}

// send a message to the queue, blocking if the message queue is full
void MessageQueue_send(MessageQueue* mq, Message* msg) {
  if (!mq || !msg || mq->status != MQ_OPEN)
    return;
    
    while (mq->current_messages >= mq->max_messages) {
      if(mq->status != MQ_OPEN){  //if the message queue is not open, return an error
        running->syscall_retvalue = DSOS_EERROR;
        return;
      }
      PCB* current = running;
      current->status = Waiting;
      List_insert(&mq->waiting_senders, mq->waiting_senders.last, (ListItem*)current);
      disastrOS_schedule();
}
  
  List_insert(&mq->messages, mq->messages.last, (ListItem*)msg); //add message to the queue
  mq->current_messages++;

  // if a receiver is blocked, wake him up
  if (mq->waiting_receivers.first) {
    PCB* pcb = (PCB*)List_detach(&mq->waiting_receivers, mq->waiting_receivers.first);
    pcb->status = Ready;
    List_insert(&ready_list, ready_list.last, (ListItem*) pcb);
  }
}

// receive a message from the queue, blocking if the message queue is empty
Message* MessageQueue_receive(MessageQueue* mq) {
  if (!mq)
    return 0;

  
   // if queue is empty, block the current process
    while (!mq->messages.first) {
    if (mq->status != MQ_OPEN){
      return 0;
    }
    PCB* current = running;
    current->status = Waiting;
    List_insert(&mq->waiting_receivers, mq->waiting_receivers.last, (ListItem*) current);
    disastrOS_schedule();
  }

  // there is a message, take it
  Message* msg = (Message*)List_detach(&mq->messages, mq->messages.first);
mq->current_messages--;

// if a sender is blocked, wake him up
if (mq->waiting_senders.first){
  PCB* pcb = (PCB*) List_detach(&mq->waiting_senders, mq->waiting_senders.first);
  pcb->status = Ready;
  List_insert(&ready_list, ready_list.last, (ListItem*) pcb);
}

  return msg;
}

// destroy a message queue, after waking up all blocked processes
void MessageQueue_destroy(MessageQueue* mq) {
  if (!mq || mq->status == MQ_CLOSED)
    return;
  
  mq->status = MQ_CLOSING;

  // wake up all blocked receivers
  while (mq->waiting_receivers.first) {
    PCB* pcb = (PCB*)List_detach(&mq->waiting_receivers, mq->waiting_receivers.first);
    pcb->status = Ready;
    pcb->syscall_retvalue = DSOS_EERROR;
    List_insert(&ready_list, ready_list.last, (ListItem*)pcb);
  }

  //wake up all blocked senders
   while (mq->waiting_senders.first) {
    PCB* pcb = (PCB*)List_detach(&mq->waiting_senders, mq->waiting_senders.first);
    pcb->status = Ready;
    pcb->syscall_retvalue = DSOS_EERROR;
    List_insert(&ready_list, ready_list.last, (ListItem*)pcb);
  }

  //free the remaining messages
 while (mq->messages.first) {
    Message* msg = (Message*)List_detach(&mq->messages, mq->messages.first);
    Message_free(msg);
  }

  mq->status = MQ_CLOSED;

  List_detach(&resources_list, (ListItem*)&mq->resource); // remove the message queue from the global resource list

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

// generate a unique message queue id
static int last_mq_id = 0;
int generate_unique_mq_id() {
    return ++last_mq_id;
}


