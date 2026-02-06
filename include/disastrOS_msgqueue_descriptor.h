#pragma once

#include "linked_list.h"
#include "disastrOS_pcb.h"
#include "disastrOS_message_queue.h"


struct MsgQueueDescriptorPtr;

// A MsgQueueDescriptor represents a per-process handle
// to a message queue.
typedef struct MsgQueueDescriptor {
  ListItem list;                       
  int fd;                              
  PCB* pcb;                            
  MessageQueue* queue;                 
  struct MsgQueueDescriptorPtr* ptr;   // pointer to entry in queue descriptor list
} MsgQueueDescriptor;

// Pointer stored in the message queue to reference
// a process descriptor (used for fast removal).
typedef struct MsgQueueDescriptorPtr {
  ListItem list;                       // list node for queue descriptor list
  MsgQueueDescriptor* descriptor;      // back-reference to descriptor
} MsgQueueDescriptorPtr;


void MsgQueueDescriptor_init();  // init function

MsgQueueDescriptor* MsgQueueDescriptor_alloc(int fd, MessageQueue* queue, PCB* pcb); // alloc function

int MsgQueueDescriptor_free(MsgQueueDescriptor* d); // free function

MsgQueueDescriptor* MsgQueueDescriptorList_byFd(ListHead* l, int fd); // finds a msgqueuedescriptor by fd

void MsgQueueDescriptorList_print(ListHead* l); //debug

// Descriptor pointer helpers
MsgQueueDescriptorPtr* MsgQueueDescriptorPtr_alloc(
  MsgQueueDescriptor* descriptor
);

int MsgQueueDescriptorPtr_free(MsgQueueDescriptorPtr* p);

void MsgQueueDescriptorPtrList_print(ListHead* l);
