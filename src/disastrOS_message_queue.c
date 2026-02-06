#include "disastrOS_message_queue.h"
#include "disastrOS.h"
#include "disastrOS_syscalls.h"

#include "linked_list.h"
#include "disastrOS_message.h"
#include "pool_allocator.h"

#define MESSAGE_QUEUE_SIZE sizeof(MessageQueue)
#define MESSAGE_QUEUE_BUFFER_SIZE MESSAGE_QUEUE_SIZE*MAX_MESSAGE_QUEUES  //dubbio: devo considerare anche la dimensione dei messaggi?

static char _message_queue_buffer[MESSAGE_QUEUE_BUFFER_SIZE];
/*
 * Pool allocator per le message queue
 */
static PoolAllocator message_queue_allocator;

/*
 * Inizializzazione globale del sottosistema MQ
 */
void message_queue_init() {
  PoolAllocatorResult result = PoolAllocator_init(&message_queue_allocator,
                               MESSAGE_QUEUE_SIZE, 
                               MAX_MESSAGE_QUEUES,
                               _message_queue_buffer,
                               MESSAGE_QUEUE_BUFFER_SIZE);
    assert(!result);  //
}

MessageQueue* MessageQueue_create(int max_messages) {
  MessageQueue* mq = (MessageQueue*)PoolAllocator_getBlock(&message_queue_allocator);

  if (!mq)
    return 0;

  List_init(&mq->messages);
  List_init(&mq->waiting_receivers);
  List_init(&mq->waiting_senders);

  mq->max_messages = MAX_MESSAGES_PER_QUEUE;
  mq->current_messages = 0;

  return mq;
}

void MessageQueue_send(MessageQueue* mq, Message* msg, PCB* sender) {
  if (!mq || !msg)
    return;

    if (mq->current_messages >= mq->max_messages) {
    PCB_block(sender);
    List_insert(&mq->waiting_senders, mq->waiting_senders.last, (ListItem*)sender);
    disastrOS_schedule();
}

  List_insert(&mq->messages,
              mq->messages.last,
              (ListItem*)msg);

    mq->current_messages++;

  /*
   * Se qualcuno stava aspettando, sveglialo
   */
  if (mq->waiting_receivers.first) {
    PCB* pcb = (PCB*)List_detach(&mq->waiting_receivers,
                                 mq->waiting_receivers.first);
    PCB_unblock(pcb);
  }
}


Message* MessageQueue_receive(MessageQueue* mq, PCB* receiver) {
  if (!mq)
    return 0;

  /*
   * Coda vuota → blocca il processo corrente
   */
  if (!mq->messages.first) {
    PCB* current = running;
    PCB_block(current);

    List_insert(&mq->waiting_receivers,
            mq->waiting_receivers.last,
            (ListItem*) current);

    disastrOS_schedule();
  }

  /*
   * A questo punto un messaggio c'è
   */
  Message* msg =
      (Message*)List_detach(&mq->messages,
                            mq->messages.first);
mq->current_messages--;

  return msg;
}

void MessageQueue_destroy(MessageQueue* mq) {
  if (!mq)
    return;

  /*
   * Libera eventuali messaggi rimasti
   */
  ListItem* it = mq->messages.first;
  while (it) {
    ListItem* next = it->next;
    Message_free((Message*)it);
    it = next;
  }

  PoolAllocator_releaseBlock(&message_queue_allocator, mq);
}

