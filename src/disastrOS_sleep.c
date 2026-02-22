#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include "disastrOS.h"
#include "disastrOS_syscalls.h"

void internal_sleep(){
  //! determines how many cicles a process has to sleep, by reading the argument from
  //! the syscall
  PCB* caller = running;

  if (caller->timer) {
    printf("process has already a timer!!!\n");
    caller->syscall_retvalue=DSOS_ESLEEP;
    return;
  }
  int cycles_to_sleep=caller->syscall_args[0];
  int wake_time=disastrOS_time+cycles_to_sleep;
  
  TimerItem* new_timer=TimerList_add(&timer_list, wake_time, caller);
  if (! new_timer) {
    printf("no new timer!!!\n");
    caller->syscall_retvalue=DSOS_ESLEEP;
    return;
  }

  caller->syscall_retvalue = 0; // success for caller when it is resumed
  caller->status=Waiting;
  caller->timer = new_timer;
  
  List_insert(&waiting_list, waiting_list.last, (ListItem*) caller);
  
  internal_schedule();  // let the scheduler pick the next ready process

  // If we switched away, do not touch another process state by mistake.
  if (running != caller)
    return;
}


