#include <stdio.h>
#include <string.h>
 
#include "disastrOS.h"
#include "disastrOS_message_queue.h"
#include "disastrOS_msgqueuetest.h"
 
#define TEST_QUEUE_ID 200
#define QUEUE_CAPACITY 2
#define MAX_MSG_SIZE 64

typedef struct {
  int queue_id;
  int sender_id;
} SenderArgs;

 static void dump_queue_state(const char* phase, int queue_id) {
  printf("\n===== %s =====\n", phase);
  MessageQueue_print_status(queue_id);
  MessageQueue_print_messages(queue_id);
  disastrOS_printStatus();
  printf("========================\n\n");
 }
 


static void oneShotSender(void* args) {
  SenderArgs* s_args = (SenderArgs*) args;
  char payload[MAX_MSG_SIZE];
  snprintf(payload,
           sizeof(payload),
           "msg[s%d] from pid=%d",
           s_args->sender_id,
           disastrOS_getpid());

  int ret = disastrOS_mq_send(s_args->queue_id, payload, (int) strlen(payload) + 1);
  printf("[SENDER %d | pid=%d] send ret=%d payload='%s'\n",
         s_args->sender_id,
         disastrOS_getpid(),
         ret,
         payload);
  MessageQueue_print_status(s_args->queue_id);

  disastrOS_exit(ret);
}
static int wait_one_child(const char* phase, int queue_id) {
  int retval = 0;
  int pid = disastrOS_wait(0, &retval);
  if (pid < 0) {
    printf("[INIT] wait failed in phase '%s'\n", phase);
    return -1;
  }
  printf("[INIT][%s] child pid=%d terminated with retval=%d\n", phase, pid, retval);
  dump_queue_state("STATO DOPO TERMINAZIONE FIGLIO", queue_id);
  return pid;
}
 
static void initFunction(void* args) {
  (void) args;
  MsgQueueTest_runAll();
 
  printf("[INIT] pid=%d, test blocking sender + unblocking via receive\n", disastrOS_getpid());
 
  int qid = disastrOS_mq_create(TEST_QUEUE_ID, QUEUE_CAPACITY);
  printf("[INIT] create queue id=%d (requested id=%d, capacity=%d)\n",
         qid,
         TEST_QUEUE_ID,
         QUEUE_CAPACITY);

  if (qid < 0) {  
    printf("[INIT] errore creazione coda, shutdown\n");
    disastrOS_shutdown();
    return;
   }
 
  dump_queue_state("QUEUE CREATA", qid);
 
   // 1) Saturiamo la coda con due sender (ognuno manda 1 messaggio)
  static SenderArgs fill_args[QUEUE_CAPACITY];
  for (int i = 0; i < QUEUE_CAPACITY; ++i) {
    fill_args[i].queue_id = qid;
    fill_args[i].sender_id = i;
    disastrOS_spawn(oneShotSender, &fill_args[i]);
    printf("[INIT] spawn filler sender[%d]\n", i);
  }

  for (int i = 0; i < QUEUE_CAPACITY; ++i) {
    if (wait_one_child("FILL_QUEUE", qid) < 0) {
      disastrOS_shutdown();
      return;
    }
  }

  dump_queue_state("CODA SATURA", qid);

  // 2) Spawn di un sender aggiuntivo: deve bloccarsi in waiting_senders
  static SenderArgs blocking_arg;
  blocking_arg.queue_id = qid;
  blocking_arg.sender_id = 99;
  disastrOS_spawn(oneShotSender, &blocking_arg);
  printf("[INIT] spawn blocking sender\n");

  // Diamo CPU al sender così tenta send su coda piena e va in waiting_senders
  disastrOS_preempt();
  dump_queue_state("SENDER BLOCCATO IN WAITING_SENDERS", qid);

  // 3) Il processo init riceve un messaggio: libera uno slot e risveglia il sender bloccato
  char out[MAX_MSG_SIZE];
  memset(out, 0, sizeof(out));
  int recv_ret = disastrOS_mq_receive(qid, out, sizeof(out));
  printf("[INIT] receive per sbloccare sender -> ret=%d payload='%s'\n", recv_ret, out);
  dump_queue_state("DOPO RECEIVE (SLOT LIBERATO)", qid);

  // Lasciamo eseguire il sender risvegliato, che completa la send e termina
  disastrOS_preempt();
  if (wait_one_child("WAIT_BLOCKING_SENDER", qid) < 0) {
    disastrOS_shutdown();
    return;
  }

  // 4) Svuotiamo la coda rimanente
  for (int i = 0; i < QUEUE_CAPACITY; ++i) {
    memset(out, 0, sizeof(out));
    recv_ret = disastrOS_mq_receive(qid, out, sizeof(out));
    printf("[INIT] drain receive #%d -> ret=%d payload='%s'\n", i, recv_ret, out);
  }

  dump_queue_state("CODA VUOTA", qid);

int destroy_ret = disastrOS_mq_destroy(qid);
  printf("[INIT] destroy queue ret=%d\n", destroy_ret);
  disastrOS_printStatus();
  printf("[INIT] shutdown\n");
  disastrOS_shutdown();
 }
 
int main(int argc, char** argv) {
  char* logfilename = 0;
  if (argc > 1) {
    logfilename = argv[1];
   }

  printf("[MAIN] start disastrOS message queue integration test\n");
  disastrOS_start(initFunction, 0, logfilename);
  return 0;
}
 