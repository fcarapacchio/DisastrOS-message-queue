#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "disastrOS.h"
#include "disastrOS_syscalls.h"
#include "disastrOS_descriptor.h"

void internal_fork() {
  PCB* new_pcb=PCB_alloc();
  if (!new_pcb) {
    running->syscall_retvalue=DSOS_ESPAWN;
    return;
  }

  new_pcb->status=Ready;

  // sets the parent of the newly created process to the running process
  new_pcb->parent=running;
  
  // adds a pointer to the new process to the children list of running
  PCBPtr* new_pcb_ptr=PCBPtr_alloc(new_pcb);
  assert(new_pcb_ptr);
  List_insert(&running->children, running->children.last, (ListItem*) new_pcb_ptr);

  //=== Duplicate descriptors ===
  ListItem* aux = running->descriptors.first;
  while(aux){
    Descriptor* parent_des = (Descriptor*) aux;
    aux = aux->next;

  Resource* res = parent_des->resource; 

   // Create a new descriptor for the child
    Descriptor* child_des = Descriptor_alloc(parent_des->fd, res, new_pcb);
    assert(child_des);

    DescriptorPtr* child_desptr = DescriptorPtr_alloc(child_des);
    assert(child_desptr);

  // Link descriptor and pointer
    child_des->ptr=child_desptr;
    child_desptr->descriptor = child_des;

  // insert into child's descriptor list
    List_insert(&new_pcb->descriptors, new_pcb->descriptors.last, (ListItem*) child_des);

  // insert descriptor pointer into resource
    List_insert(&res->descriptors_ptrs, res->descriptors_ptrs.last, (ListItem*) child_desptr);

    res->ref_count++;
  }

  // sets fork return values
  running->syscall_retvalue=new_pcb->pid;

  // adds the new process to the ready queue
  List_insert(&ready_list, ready_list.last, (ListItem*) new_pcb);
}
