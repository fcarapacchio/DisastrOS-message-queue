CC=gcc
CCOPTS=--std=gnu99 -Wall -Ofast -D_DISASTROS_DEBUG_ -Iinclude
AR=ar

HEADERS=include/disastrOS.h \
	include/disastrOS_constants.h \
	include/disastrOS_globals.h \
	include/disastrOS_pcb.h \
	include/disastrOS_syscalls.h \
	include/disastrOS_timer.h \
	include/disastrOS_resource.h \
	include/disastrOS_descriptor.h \
	include/disastrOS_message.h \
	include/disastrOS_message_queue.h \
	include/disastrOS_msgqueuetest.h \
	include/linked_list.h \
	include/pool_allocator.h

OBJS=pool_allocator.o \
	linked_list.o \
	disastrOS_timer.o \
	disastrOS_pcb.o \
	disastrOS_resource.o \
	disastrOS_descriptor.o \
	disastrOS_message.o \
	disastrOS_message_queue.o \
	disastrOS.o \
	disastrOS_wait.o \
	disastrOS_fork.o \
	disastrOS_spawn.o \
	disastrOS_exit.o \
	disastrOS_sleep.o \
	disastrOS_shutdown.o \
	disastrOS_schedule.o \
	disastrOS_preempt.o \
	disastrOS_open_resource.o \
	disastrOS_close_resource.o \
	disastrOS_destroy_resource.o \
	disastrOS_msgqueue_create.o \
	disastrOS_msgqueue_send.o \
	disastrOS_msgqueue_receive.o \
	disastrOS_msgqueue_destroy.o \
	disastrOS_msgqueue_test.o

LIBS=libdisastrOS.a
BINS=disastrOS_test

.PHONY: clean all run

all: $(LIBS) $(BINS)

%.o: src/%.c $(HEADERS)
	$(CC) $(CCOPTS) -c -o $@ $<

$(LIBS): $(OBJS)
	$(AR) -rcs $@ $^
	$(RM) $(OBJS)

$(BINS): src/disastrOS_test.c $(LIBS) $(HEADERS)
	$(CC) $(CCOPTS) -o $@ $< -L. -ldisastrOS

run: $(BINS)
	./$(BINS)

clean:
	rm -rf *.o *~ $(LIBS) $(BINS)