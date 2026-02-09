#include "disastrOS_message.h"
#include "pool_allocator.h"
#include "disastrOS_constants.h"
#include "disastrOS.h"
#include "disastrOS_syscalls.h"

#include <string.h>
#include <stdio.h>


// max number of simultaneous messages
#ifndef DSOS_MAX_MESSAGES
#define DSOS_MAX_MESSAGES 128
#endif


static char messages_buffer[
  DSOS_MAX_MESSAGES * (MESSAGE_SIZE + MESSAGE_MAX_SIZE)
];

// message pool allocator
static PoolAllocator message_allocator;

/*
 * message initialization
 */
void Message_init() {
  PoolAllocator_init(
    &message_allocator,
    MESSAGE_SIZE + MESSAGE_MAX_SIZE,
    DSOS_MAX_MESSAGES,
    messages_buffer,
    sizeof(messages_buffer)
  );
}


Message* Message_alloc(int size) {
  if (size <= 0 || size > MESSAGE_MAX_SIZE)
    return 0;

  Message* msg = (Message*) PoolAllocator_getBlock(&message_allocator);
  if (!msg)
    return 0;

  msg->size = size;
  msg->data = (char*)(msg + 1);

  memset(msg->data, 0, size);

  msg->list.prev = 0;
  msg->list.next = 0;

  msg->sender = running;

  return msg;
}

void Message_free(Message* msg) {
  if (!msg)
    return;

  PoolAllocator_releaseBlock(&message_allocator, msg);
}

void Message_print(Message* msg) {
  if (!msg) {
    printf("Message: (null)\n");
    return;
  }

  printf("Message @%p | size=%d | data=%p\n",
         msg, msg->size, msg->data);
}
