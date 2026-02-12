#pragma once
#include <assert.h>
#include "disastrOS.h"
#include "disastrOS_globals.h"

// process management
void internal_preempt();
void internal_fork();
void internal_exit();
void internal_wait();
void internal_spawn();
void internal_shutdown();
void internal_schedule();
void internal_sleep();

// resource management
void internal_openResource();
void internal_closeResource();
void internal_destroyResource();

// message queue internal syscalls
void internal_mq_create();
void internal_mq_send();
void internal_mq_receive();
void internal_mq_destroy();

