#include "disastrOS_message_queue.h"
#include "disastrOS.h"
#include "disastrOS_syscalls.h"
#include <stdio.h>
#include "linked_list.h"
#include "disastrOS_message.h"
#include "pool_allocator.h"

static char _message_queue_buffer[MESSAGE_QUEUE_BUFFER_SIZE];
/*
 * message queue pool allocator
 */
static PoolAllocator message_queue_allocator;

/*
 * message queue initialization
 */
void MessageQueue_init() {
  PoolAllocatorResult result = PoolAllocator_init(&message_queue_allocator,
                               MESSAGE_QUEUE_SIZE, 
                               MAX_MESSAGE_QUEUES,
                               _message_queue_buffer,
                               MESSAGE_QUEUE_BUFFER_SIZE);
    assert(!result);  
}

MessageQueue* MessageQueue_alloc(int queue_id, int size) {

  MessageQueue* mq = (MessageQueue*) PoolAllocator_getBlock(&message_queue_allocator);

  if (!mq)
    return 0;

  // Resource part
  mq->resource.list.prev = mq->resource.list.next = 0;
  mq->resource.id = queue_id;
  mq->resource.type = RESOURCE_MESSAGE_QUEUE;
  mq->resource.ref_count = 0;
  List_init(&mq->resource.descriptors_ptrs);

  // init internal list
  mq->list.prev = mq->list.next = 0;
  
  // Message Queue part
  List_init(&mq->messages);
  List_init(&mq->waiting_receivers);
  List_init(&mq->waiting_senders);

  mq->max_messages = size;
  mq->current_messages = 0;
  mq->queue_id = queue_id;
  mq->status = MQ_OPEN;

  return mq;
}


void MessageQueue_free(MessageQueue* mq) {
  assert(mq->waiting_receivers.first == 0); // there are no processes blocked in receive
  assert(mq->waiting_senders.first == 0);   // there are no processes blocked in send
  assert(mq->messages.first == 0);          // message queue is empty
  assert(mq->resource.descriptors_ptrs.first == 0);

  PoolAllocator_releaseBlock(&message_queue_allocator, mq);
  return;
}


void MessageQueue_printAll() {

  printf("=== Message Queues ===\n");

  ListItem* item = resources_list.first;

  int found = 0;

  while (item) {
    Resource* r = (Resource*) item;

    if (r->type == RESOURCE_MESSAGE_QUEUE) {

      MessageQueue* mq = (MessageQueue*) r;

      printf("Queue ID: %d | status: %d | messages: %d/%d\n", mq->queue_id, mq->status, mq->current_messages, mq->max_messages);

      found++;
    }

    item = item->next;
  }

  if (!found)
    printf("No message queues present.\n");

  printf("======================\n");
}

void MessageQueue_print_messages(int queue_id) {
  if (!queue_id) {
    printf("MessageQueue_print_messages: NULL queue\n");
    return;
  }

  ListItem* item = resources_list.first;
  while (item) {
    Resource* r = (Resource*) item;

    if (r->type == RESOURCE_MESSAGE_QUEUE && r->id == queue_id) {
      MessageQueue* mq = (MessageQueue*) r;
      printf("Queue %d messages (%d/%d):\n", mq->queue_id, mq->current_messages, mq->max_messages);

      if (!mq->messages.first) {
         printf("  [empty]\n");
         return;
      }

     ListItem* it = mq->messages.first;
     int index = 0;

     while (it) {
      Message* msg = (Message*) it;
      printf("  [%d] sender=%d size=%d\n", index, msg->sender->pid, msg->size);
      it = it->next;
      index++;
    }
  }
  item = item->next;
}

}

void MessageQueue_print(int queue_id){

  ListItem* item = resources_list.first;

  while (item) {
    Resource* r = (Resource*) item;

    if (r->type == RESOURCE_MESSAGE_QUEUE && r->id == queue_id) {
      MessageQueue* mq = (MessageQueue*) r;
      printf("Message Queue: id=%d, max=%d, current=%d, status=%d\n", mq->queue_id, mq->max_messages, mq->current_messages, mq->status);
      printf("Messages:\n");
      ListItem* it = mq->messages.first;
      while(it) {
        Message* msg = (Message*)it;
        printf("  size=%d, data=%s\n", msg->size, msg->data);
        it = it->next;
      }

      printf("Waiting receivers:\n");
      int wr_count=0;
      it = mq->waiting_receivers.first;
      while(it) {
        PCB* pcb = (PCB*)it;
        printf("  PID=%d\n", pcb->pid);
        it = it->next;
        wr_count++;
      }
      if (!wr_count)
        printf("  [none]\n");
      printf("  total_waiting_receivers=%d\n", wr_count);

      printf("Waiting senders:\n");
      int ws_count=0;
      it = mq->waiting_senders.first;
      while(it) {
        PCB* pcb = (PCB*)it;
        printf("  PID=%d\n", pcb->pid);
        it = it->next;
        ws_count++;
      }
      if (!ws_count)
        printf("  [none]\n");
      printf("  total_waiting_senders=%d\n", ws_count);
    }
    item = item->next;
  }

}







