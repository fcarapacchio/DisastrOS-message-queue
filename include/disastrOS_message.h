#pragma once

#include "linked_list.h"
#include "disastrOS_pcb.h"

// A Message is a single object that can be inserted in a message queue.
// MUST: ListItem must be the first field to allow list usage

typedef struct Message {
  ListItem list;   // list node
  int size;        // payload size
  char* data;      // payload buffer
  PCB* sender;     // process that sent the message
} Message;

// Initializes the message subsystem
void Message_init();

// Allocates a message with a payload of 'size' bytes
Message* Message_alloc(int size);

// Frees an allocated message
void Message_free(Message* msg);

// Debug function
void Message_print(Message* msg);
