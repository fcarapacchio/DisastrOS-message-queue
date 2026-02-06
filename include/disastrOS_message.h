#pragma once

#include "linked_list.h"

// Un Message rappresenta un singolo messaggio
// inseribile in una message queue.
// MUST: ListItem come primo campo per l'uso nelle liste.

typedef struct Message {
  ListItem list;   // nodo di lista (per la queue)
  int size;        // dimensione del payload in byte
  char* data;      // payload
} Message;

// Inizializza il sottosistema messaggi (pool allocator)
void Message_init();

// Alloca un messaggio con payload di size byte
// Ritorna NULL se non c'è memoria
Message* Message_alloc(int size);

// Libera un messaggio precedentemente allocato
void Message_free(Message* msg);

// Funzione di debug
void Message_print(Message* msg);
