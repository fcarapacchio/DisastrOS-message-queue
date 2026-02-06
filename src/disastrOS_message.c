#include "disastrOS_message.h"
#include "pool_allocator.h"
#include "disastrOS_constants.h"

#include <string.h>
#include <stdio.h>

// ----------------------
// Pool allocator setup
// ----------------------

// Numero massimo di messaggi simultanei
#ifndef DSOS_MAX_MESSAGES
#define DSOS_MAX_MESSAGES 128
#endif

// Dimensione massima di un messaggio
#ifndef DSOS_MAX_MESSAGE_SIZE
#define DSOS_MAX_MESSAGE_SIZE 256
#endif

// Buffer di memoria per i messaggi
static char messages_buffer[
  DSOS_MAX_MESSAGES * (sizeof(Message) + DSOS_MAX_MESSAGE_SIZE)
];

// Pool allocator globale per i Message
static PoolAllocator message_allocator;

// ----------------------
// Inizializzazione
// ----------------------

void Message_init() {
  PoolAllocator_init(
    &message_allocator,
    sizeof(Message) + DSOS_MAX_MESSAGE_SIZE,
    DSOS_MAX_MESSAGES,
    messages_buffer,
    sizeof(messages_buffer)
  );
}

// ----------------------
// Allocazione
// ----------------------

Message* Message_alloc(int size) {
  if (size <= 0 || size > DSOS_MAX_MESSAGE_SIZE)
    return 0;

  Message* msg = (Message*) PoolAllocator_getBlock(&message_allocator);
  if (!msg)
    return 0;

  // Il payload è subito dopo la struct Message
  msg->size = size;
  msg->data = (char*)(msg + 1);

  // Azzeriamo il payload per sicurezza
  memset(msg->data, 0, size);

  // inizializza i puntatori di lista
  msg->list.prev = 0;
  msg->list.next = 0;

  return msg;
}

// ----------------------
// Deallocazione
// ----------------------

void Message_free(Message* msg) {
  if (!msg)
    return;

  PoolAllocator_releaseBlock(&message_allocator, msg);
}

// ----------------------
// Debug
// ----------------------

void Message_print(Message* msg) {
  if (!msg) {
    printf("Message: (null)\n");
    return;
  }

  printf("Message @%p | size=%d | data=%p\n",
         msg, msg->size, msg->data);
}
