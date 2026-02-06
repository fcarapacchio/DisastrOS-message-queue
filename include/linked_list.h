#pragma once

typedef struct ListItem {
    struct ListItem* next;
    struct ListItem* prev;
} ListItem;

typedef struct ListHead {
    ListItem* first;
    ListItem* last;
    int size;
} ListHead;

void List_init(ListHead* list);

ListItem* List_find(ListHead* head, ListItem* item);

ListItem* List_insert(ListHead* head, ListItem* previous, ListItem* item); // inserts an item at the end of the list

ListItem* List_detach(ListHead* list, ListItem* item); // removes an item from the list

ListItem* List_first(ListHead* list); // returns (without removing) the first item of the list

ListItem* List_next(ListItem* item); // returns the next item of the list

int List_isEmpty(ListHead* list);

void List_print(ListHead* list, void (*printItem)(ListItem*));
