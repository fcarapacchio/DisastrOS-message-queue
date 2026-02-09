#include "disastrOS_msgqueue_descriptor.h"
#include "pool_allocator.h"
#include <stdio.h>
#include <assert.h>

#define MSGQUEUE_DESCRIPTOR_SIZE sizeof(MsgQueueDescriptor)
#define MSGQUEUE_DESCRIPTOR_PTR_SIZE sizeof(MsgQueueDescriptorPtr)
#define MSGQUEUE_DESCRIPTOR_BUFFER_SIZE MSGQUEUE_DESCRIPTOR_SIZE*MAX_NUM_PROCESSES
#define MSGQUEUE_DESCRIPTOR_PTR_BUFFER_SIZE MSGQUEUE_DESCRIPTOR_PTR_SIZE*MAX_NUM_PROCESSES

static char _msgqueue_descriptor_buffer[MSGQUEUE_DESCRIPTOR_BUFFER_SIZE];
static char _msgqueue_descriptor_ptr_buffer[MSGQUEUE_DESCRIPTOR_PTR_BUFFER_SIZE];

static PoolAllocator _msgqueue_descriptor_allocator;
static PoolAllocator _msgqueue_descriptor_ptr_allocator;

/*
=== Initialization ===
*/
void MsgQueueDescriptor_init(){
  int result = PoolAllocator_init(
            &_msgqueue_descriptor_allocator,
            MSGQUEUE_DESCRIPTOR_SIZE,
            MAX_NUM_PROCESSES,
            _msgqueue_descriptor_buffer,
            MSGQUEUE_DESCRIPTOR_BUFFER_SIZE
            );
  assert(!result);

  result = PoolAllocator_init(
        &_msgqueue_descriptor_ptr_allocator,
        MSGQUEUE_DESCRIPTOR_PTR_SIZE,
        MAX_NUM_PROCESSES,
        _msgqueue_descriptor_ptr_buffer,
        MSGQUEUE_DESCRIPTOR_PTR_BUFFER_SIZE
        );
  assert(!result);
}

/*
=== Descriptor allocation / free ===
*/
MsgQueueDescriptor* MsgQueueDescriptor_alloc(int fd, MessageQueue* queue, PCB* pcb){
  MsgQueueDescriptor* d = (MsgQueueDescriptor*) PoolAllocator_getBlock(&_msgqueue_descriptor_allocator);
  if (!d) return 0;

  d->fd = fd;
  d->pcb = pcb;
  d->queue = queue;
  d->ptr = MsgQueueDescriptorPtr_alloc(d); // allocate back-pointer

  d->list.prev = 0;
  d->list.next = 0;

  return d;
}

int MsgQueueDescriptor_free(MsgQueueDescriptor* d){
  if (!d) return -1;

  if (d->ptr) {
    MsgQueueDescriptorPtr_free(d->ptr);
    d->ptr = 0;
  }

  return PoolAllocator_releaseBlock(&_msgqueue_descriptor_allocator, d);
}

/*
=== Descriptor pointer allocation / free ===
*/
MsgQueueDescriptorPtr* MsgQueueDescriptorPtr_alloc(MsgQueueDescriptor* descriptor){
  MsgQueueDescriptorPtr* p = (MsgQueueDescriptorPtr*) PoolAllocator_getBlock(&_msgqueue_descriptor_ptr_allocator);
  if (!p) return 0;

  p->descriptor = descriptor;
  p->list.prev = 0;
  p->list.next = 0;

  return p;
}

int MsgQueueDescriptorPtr_free(MsgQueueDescriptorPtr* p){
  if (!p) return -1;
  return PoolAllocator_releaseBlock(&_msgqueue_descriptor_ptr_allocator, p);
}

/*
=== Descriptor list helpers ===
*/
MsgQueueDescriptor* MsgQueueDescriptorList_byFd(ListHead* l, int fd){
  ListItem* aux = l->first;
  while(aux){
    MsgQueueDescriptor* d = (MsgQueueDescriptor*) aux;
    if (d->fd == fd) return d;
    aux = aux->next;
  }
  return 0;
}

void MsgQueueDescriptorList_print(ListHead* l){
  ListItem* aux = l->first;
  printf("[");
  while(aux){
    MsgQueueDescriptor* d = (MsgQueueDescriptor*) aux;
    printf("(fd:%d pcb:%p queue:%p)", d->fd, d->pcb, d->queue);
    aux = aux->next;
    if (aux) printf(", ");
  }
  printf("]\n");
}

void MsgQueueDescriptorPtrList_print(ListHead* l){
  ListItem* aux = l->first;
  printf("[");
  while(aux){
    MsgQueueDescriptorPtr* p = (MsgQueueDescriptorPtr*) aux;
    printf("(descriptor:%p)", p->descriptor);
    aux = aux->next;
    if (aux) printf(", ");
  }
  printf("]\n");
}
