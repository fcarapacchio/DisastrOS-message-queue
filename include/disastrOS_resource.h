#pragma once
#include "linked_list.h"
#include "disastrOS_pcb.h"

typedef enum {
  RESOURCE_GENERIC = 0,
  RESOURCE_MESSAGE_QUEUE
} ResourceType;

typedef struct {
  ListItem list;
  int id;
  ResourceType type;
  int ref_count;    //number of open descriptors
  void* resource_data;   // pointer to resource-specific data (like a MessageQueue)
  ListHead descriptors_ptrs;
} Resource;

void Resource_init();

Resource* Resource_alloc(int id, int type);
int Resource_free(Resource* resource);

typedef ListHead ResourceList;

Resource* ResourceList_byId(ResourceList* l, int id);

void ResourceList_print(ListHead* l);