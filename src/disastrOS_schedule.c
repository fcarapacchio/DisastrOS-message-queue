#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include "disastrOS.h"
#include "disastrOS_syscalls.h"

// replaces the running process with the next one in the ready list
void internal_schedule() {
  if (running) {
    disastrOS_debug("SCHEDULE - %d ->", running->pid);
   }


// we check the timers, to see if to wake up some process
  // we insert the processes in front the ready list
  TimerItem* elapsed_timer=0;
  PCB* previous_pcb=0;
  


  while( (elapsed_timer=TimerList_current(&timer_list, disastrOS_time)) ){
    PCB* pcb_to_wake=elapsed_timer->pcb;
    List_detach(&waiting_list, (ListItem*) pcb_to_wake);
    pcb_to_wake->status=Ready;
    pcb_to_wake->timer=0;
    List_insert(&ready_list, (ListItem*) previous_pcb, (ListItem*) pcb_to_wake);
    previous_pcb=pcb_to_wake;
    TimerList_removeCurrent(&timer_list);
  } 
// if there is no ready process, do not preempt
if(!ready_list.first) return;

PCB* next_process = (PCB*) List_detach(&ready_list, ready_list.first);
assert(next_process);

// reinsert the running process only if it was actually running
if (running && running->status == Running){
  running->status = Ready;
  List_insert(&ready_list, ready_list.last, (ListItem*) running);
}

// switch to the new process
next_process->status = Running;
running = next_process;
  
disastrOS_debug(" %d\n", running->pid);

}
