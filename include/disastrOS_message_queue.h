#pragma once

#include "linked_list.h"
#include "disastrOS_pcb.h"
#include "disastrOS_message.h"
#include "disastrOS_resource.h"

// Message queue status
typedef enum {
  MQ_INVALID = -1,   
  MQ_OPEN    = 0,    
  MQ_CLOSING = 1,   
  MQ_CLOSED  = 2     
} MQStatus;

// Message Queue kernel object
typedef struct MessageQueue {

  // MUST BE FIRST if treated as a Resource
  Resource resource;

  // message queue global list 
  ListItem list;

  // queue message list
  ListHead messages;        

  // blocked processes in receive (queue is empty)
  ListHead waiting_receivers;

  // blocked processes in send (queue is full)
  ListHead waiting_senders;

  int max_messages;
  int current_messages;
  int queue_id;

  MQStatus status;

} MessageQueue;

// initialize message queue
void MessageQueue_init();

// create new message queue
MessageQueue* MessageQueue_create(int max_messages);

// destroy message queue
void MessageQueue_destroy(MessageQueue* mq);

// send message
void MessageQueue_send(MessageQueue* mq, Message* msg);

// receive message
Message* MessageQueue_receive(MessageQueue* mq);

// debug
void MessageQueue_print_status(MessageQueue* mq);
