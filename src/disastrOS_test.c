#include <stdio.h>
#include <string.h>
 
#include "disastrOS.h"
#include "disastrOS_message_queue.h"
#include "disastrOS_msgqueuetest.h"
 
#define TEST_QUEUE_ID 200
#define TEST_QUEUE_ID_1 200
#define TEST_QUEUE_ID_2 201
#define QUEUE_CAPACITY 4
#define MAX_MSG_SIZE 64

typedef struct {
  int queue_id;
  int sender_id;
  const char* queue_label;
} SenderArgs;

 static void dump_queue_state(const char* phase, int queue_id) {
  printf("\n===== %s =====\n", phase);
  MessageQueue_print(queue_id);
  MessageQueue_print_messages(queue_id);
  disastrOS_printStatus();
  printf("========================\n\n");
 }

static void oneShotSender(void* args) {
  SenderArgs* s_args = (SenderArgs*) args;
  char payload[MAX_MSG_SIZE];
  snprintf(payload,
           sizeof(payload),
           "msg[%s:s%d] from pid=%d",
           s_args->queue_label,
           s_args->sender_id,
           disastrOS_getpid());

  int ret = disastrOS_mq_send(s_args->queue_id, payload, (int) strlen(payload) + 1);
  printf("[SENDER %d | pid=%d] send ret=%d payload='%s'\n",
         s_args->sender_id,
         disastrOS_getpid(),
         ret,
         payload);
  MessageQueue_print(s_args->queue_id);

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
 
  printf("[INIT] pid=%d, test multi-queue with blocking sender + unblocking via receive\n", disastrOS_getpid());
 
  int qid1 = disastrOS_mq_create(TEST_QUEUE_ID_1, QUEUE_CAPACITY);
  printf("[INIT] create queue#1 id=%d (requested id=%d, capacity=%d)\n",
         qid1,
         TEST_QUEUE_ID_1,
         QUEUE_CAPACITY);

  if (qid1 < 0) {  
    printf("[INIT] errore creazione queue#1, shutdown\n");
    disastrOS_shutdown();
    return;
   }

  int qid2 = disastrOS_mq_create(TEST_QUEUE_ID_2, QUEUE_CAPACITY);
  printf("[INIT] create queue#2 id=%d (requested id=%d, capacity=%d)\n",
         qid2,
         TEST_QUEUE_ID_2,
         QUEUE_CAPACITY);

  if (qid2 < 0) {
    printf("[INIT] errore creazione queue#2, shutdown\n");
    disastrOS_mq_destroy(qid1);
    disastrOS_shutdown();
    return;
   }
 
  dump_queue_state("QUEUE#1 CREATA", qid1);
  dump_queue_state("QUEUE#2 CREATA", qid2);
 
   // 1) Saturiamo la coda con due sender (ognuno manda 1 messaggio)
  static SenderArgs fill_args[QUEUE_CAPACITY];
  for (int i = 0; i < QUEUE_CAPACITY; ++i) {
    fill_args[i].queue_id = qid1;
    fill_args[i].sender_id = i;
    fill_args[i].queue_label = "Q1";
    disastrOS_spawn(oneShotSender, &fill_args[i]);
    printf("[INIT] spawn filler sender Q1[%d]\n", i);
  }

  for (int i = 0; i < QUEUE_CAPACITY; ++i) {
    if (wait_one_child("FILL_QUEUE_1", qid1) < 0) {
      disastrOS_shutdown();
      return;
    }
  }

  dump_queue_state("CODA#1 SATURA", qid1);

  // 1b) Riempimento indipendente su queue#2
  static SenderArgs fill_args_q2[QUEUE_CAPACITY];
  for (int i = 0; i < QUEUE_CAPACITY; ++i) {
    fill_args_q2[i].queue_id = qid2;
    fill_args_q2[i].sender_id = i;
    fill_args_q2[i].queue_label = "Q2";
    disastrOS_spawn(oneShotSender, &fill_args_q2[i]);
    printf("[INIT] spawn filler sender Q2[%d]\n", i);
  }


  for (int i = 0; i < QUEUE_CAPACITY; ++i) {
    if (wait_one_child("FILL_QUEUE_2", qid2) < 0) {
      disastrOS_shutdown();
      return;
    }
  }

  dump_queue_state("CODA#2 SATURA", qid2);

  // 2) Spawn di un sender aggiuntivo su queue#1: deve bloccarsi in waiting_senders
  static SenderArgs blocking_arg;
  blocking_arg.queue_id = qid1;
  blocking_arg.sender_id = 99;
   blocking_arg.queue_label = "Q1";
  disastrOS_spawn(oneShotSender, &blocking_arg);
  printf("[INIT] spawn blocking sender on Q1\n");

  // Diamo CPU al sender così tenta send su coda piena (Q1) e va in waiting_senders
  disastrOS_preempt();
  dump_queue_state("Q1: SENDER BLOCCATO IN WAITING_SENDERS", qid1);
  dump_queue_state("Q2 INVARIATA", qid2);

  // 3) Il processo init riceve un messaggio da Q1: libera uno slot e risveglia il sender bloccato
  char out[MAX_MSG_SIZE];
  memset(out, 0, sizeof(out));
  int recv_ret = disastrOS_mq_receive(qid1, out, sizeof(out));
  printf("[INIT] Q1 receive per sbloccare sender -> ret=%d payload='%s'\n", recv_ret, out);
  dump_queue_state("Q1 DOPO RECEIVE (SLOT LIBERATO)", qid1);
  dump_queue_state("Q2 ANCORA PIENA", qid2);

  // Lasciamo eseguire il sender risvegliato su Q1, che completa la send e termina
  disastrOS_preempt();
  if (wait_one_child("WAIT_BLOCKING_SENDER_Q1", qid1) < 0) {
    disastrOS_shutdown();
    return;
  }

  // 4) Svuotiamo prima Q1 finché ci sono messaggi
  for (int i = 0; i < QUEUE_CAPACITY; ++i) {
    memset(out, 0, sizeof(out));
    recv_ret = disastrOS_mq_receive(qid1, out, sizeof(out));
    if (recv_ret <= 0) {
      printf("[INIT] Q1 drain stop #%d -> ret=%d\n", i, recv_ret);
      break;
    }
    printf("[INIT] Q1 drain receive #%d -> ret=%d payload='%s'\n", i, recv_ret, out);
  }

  dump_queue_state("Q1 VUOTA", qid1);

  // 5) Poi svuotiamo Q2 finché ci sono messaggi
  for (int i = 0; i < QUEUE_CAPACITY; ++i) {
    memset(out, 0, sizeof(out));
    recv_ret = disastrOS_mq_receive(qid2, out, sizeof(out));
    if (recv_ret <= 0) {
      printf("[INIT] Q2 drain stop #%d -> ret=%d\n", i, recv_ret);
      break;
    }
    printf("[INIT] Q2 drain receive #%d -> ret=%d payload='%s'\n", i, recv_ret, out);
  }

  dump_queue_state("Q2 VUOTA", qid2);

int destroy_ret_1 = disastrOS_mq_destroy(qid1);
  int destroy_ret_2 = disastrOS_mq_destroy(qid2);
  printf("[INIT] destroy queue#1 ret=%d | queue#2 ret=%d\n", destroy_ret_1, destroy_ret_2);
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
 