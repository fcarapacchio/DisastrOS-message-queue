#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include "disastrOS.h"
#include "disastrOS_syscalls.h"
#include "disastrOS_message_queue.h"

void internal_mq_create() {

  int queue_id  = running->syscall_args[0];
  int size  = running->syscall_args[1];

  // check parameter
  if (size <= 0 || size > MAX_MESSAGES_PER_QUEUE) {
    running->syscall_retvalue = DSOS_EMQINVALID;
    return;
  }

  // check if the message queue already exists
   Resource* existing = ResourceList_byId(&resources_list, queue_id);
  if (existing && existing->type == RESOURCE_MESSAGE_QUEUE) {
    running->syscall_retvalue = DSOS_EMQEXISTS;
    return;
  }

  // memory allocation
  MessageQueue* mq = MessageQueue_alloc(queue_id, size);
  if (!mq) {
    running->syscall_retvalue = DSOS_EMQCREATE;
    return;
  }

  // insert in the global list
  List_insert(&resources_list, resources_list.last, (ListItem*) &mq->resource);
  running->syscall_retvalue = queue_id;
}
