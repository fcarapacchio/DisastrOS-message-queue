#pragma once

#include "linked_list.h"
#include "disastrOS_pcb.h"
#include "disastrOS_message.h"
#include "disastrOS_resource.h"

// Stato della message queue
typedef enum {
  MQ_OPEN = 0,
  MQ_CLOSED
} MQStatus;

// Message Queue kernel object
typedef struct MessageQueue {

  // MUST BE FIRST if treated as a Resource
  Resource resource;

  // lista dei messaggi presenti nella queue
  ListHead messages;        // lista di Message

  // processi bloccati in receive
  ListHead waiting_receivers; // lista di PCBPtr o MQDescriptorPtr

  // processi bloccati in send (queue piena)
  ListHead waiting_senders;   // opzionale ma corretto

  int max_messages;         // capacità massima
  int current_messages;     // messaggi presenti

  MQStatus status;

} MessageQueue;

// inizializzazione sottosistema MQ
void MessageQueue_init();

// creazione di una nuova message queue
MessageQueue* MessageQueue_create(int max_messages);

// distruzione (fallisce se ancora in uso)
void MessageQueue_destroy(MessageQueue* mq);

// invio messaggio
// ritorna 0 se ok, <0 se errore o blocco
void MessageQueue_send(MessageQueue* mq, Message* msg, PCB* sender);

// ricezione messaggio
// ritorna Message* o NULL se bloccante
Message* MessageQueue_receive(MessageQueue* mq, PCB* receiver);

// debug
void MessageQueue_print(MessageQueue* mq);
