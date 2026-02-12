#pragma once
#include "disastrOS_pcb.h"
#include "linked_list.h"

#ifdef _DISASTROS_DEBUG_
#include <stdio.h>

#define disastrOS_debug(...) printf(__VA_ARGS__) 

#else //_DISASTROS_DEBUG_

#define disastrOS_debug(...) ;

#endif //_DISASTROS_DEBUG_

// initializes the structures and spawns a fake init process
void disastrOS_start(void (*f)(void*), void* args, char* logfile);

// generic syscall 
int disastrOS_syscall(int syscall_num, ...);

// classical process control
int disastrOS_getpid(); // this should be a syscall, but we have no memory separation, so we return just the running pid
int disastrOS_fork();
void disastrOS_exit(int exit_value);
int disastrOS_wait(int pid, int* retval);
void disastrOS_preempt();
void disastrOS_spawn(void (*f)(void*), void* args );
void disastrOS_shutdown();

// timers
void disastrOS_sleep(int);

// resources (files)
int disastrOS_openResource(int resource_id, int type, int mode);
int disastrOS_closeResource(int fd) ;
int disastrOS_destroyResource(int resource_id);

//=== MESSAGE QUEUE FUNCTIONS ===

// create/destroy a message queue
int disastrOS_mq_create(int queue_id, int max_msgs);
int disastrOS_mq_destroy(int queue_id);

// send/receive messages
int disastrOS_mq_send(int queue_id, void* msg, int size);
int disastrOS_mq_receive(int queue_id, void* msg_buffer, int size);

// debug/query queue
int disastrOS_msgQueueLength(int queue_id);  // queue size
int disastrOS_msgQueueCapacity(int queue_id);

// debug function, prints the state of the internal system
void disastrOS_printStatus();