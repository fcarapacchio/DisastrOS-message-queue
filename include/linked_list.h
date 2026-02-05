#pragma once

// Nodo generico della lista
typedef struct ListItem {
    struct ListItem* next;
    struct ListItem* prev;
} ListItem;

// Lista doppia generica
typedef struct ListHead {
    ListItem* first;
    ListItem* last;
    int size;
} ListHead;

// inizializza una lista vuota
void List_init(ListHead* list);

ListItem* List_find(ListHead* head, ListItem* item);

// inserisce un item in fondo alla lista
ListItem* List_insert(ListHead* head, ListItem* previous, ListItem* item);

// rimuove un item dalla lista
ListItem* List_detach(ListHead* list, ListItem* item);

// ritorna il primo item della lista (senza rimuovere)
ListItem* List_first(ListHead* list);

// ritorna il prossimo item nella lista
ListItem* List_next(ListItem* item);

// controlla se la lista è vuota
int List_isEmpty(ListHead* list);

// stampa lista generica (debug)
void List_print(ListHead* list, void (*printItem)(ListItem*));
