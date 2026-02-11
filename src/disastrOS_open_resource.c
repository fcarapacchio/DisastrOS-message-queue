#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include "disastrOS.h"
#include "disastrOS_syscalls.h"
#include "disastrOS_resource.h"
#include "disastrOS_descriptor.h"


void internal_openResource(){

  // get from the PCB the resource id of the resource to open
  int id=running->syscall_args[0];
  int type=running->syscall_args[1];
  int open_mode=running->syscall_args[2];

  Resource* res=ResourceList_byId(&resources_list, id);
  // if the resource is opened in CREATE mode, create the resource
  //  and return an error if the resource is already existing
  // otherwise fetch the resource from the system list, and if you don't find it
  // throw an error
  //printf ("CREATING id %d, type: %d, open mode %d\n", id, type, open_mode);
  if (open_mode&DSOS_CREATE){
    if (res) {
      running->syscall_retvalue=DSOS_ERESOURCECREATE;
      return;
    }

    res=Resource_alloc(id, type);

    if (!res) {
      running->syscall_retvalue = DSOS_ERESOURCECREATE;
      return;
    }
    List_insert(&resources_list, resources_list.last, (ListItem*) res);
  }

  // at this point we should have the resource, if not something was wrong
  if (! res || res->type!=type) {
     running->syscall_retvalue=DSOS_ERESOURCEOPEN;
     return;
  }
  
  if (open_mode&DSOS_EXCL && res->ref_count>0){
     running->syscall_retvalue=DSOS_ERESOURCENOEXCL;
     return;
  }
  
  // create the descriptor for the resource in this process, and add it to
  //  the process descriptor list. Assign to the resource a new fd
  Descriptor* des=Descriptor_alloc(running->last_fd, res, running);
  if (! des){
     running->syscall_retvalue=DSOS_ERESOURCENOFD;
     return;
  }

  // allocate descriptor pointer
  DescriptorPtr* desptr=DescriptorPtr_alloc(des);
  if (!desptr){
    Descriptor_free(des);
    running->syscall_retvalue = DSOS_ERESOURCENOFD;
    return;
  }

  des->ptr=desptr; // link descriptor with descriptor pointer
  
  // insert descriptor into process descriptor list
  List_insert(&running->descriptors, running->descriptors.last, (ListItem*) des);
  
  // insert descriptor pointer into resource descriptor list
  List_insert(&res->descriptors_ptrs, res->descriptors_ptrs.last, (ListItem*) desptr);

  res->ref_count++; //update reference counter

  running->last_fd++; // we increment the fd value for the next call
  
  running->syscall_retvalue = des->fd; // return the FD of the new descriptor to the process
}
