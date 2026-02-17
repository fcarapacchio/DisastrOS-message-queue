#include <stdio.h>
#include <string.h>
 
#include "disastrOS.h"
#include "disastrOS_message_queue.h"
 
#define TEST_QUEUE_ID 200
#define PRODUCERS 3
#define CONSUMERS 2
#define MSGS_PER_PRODUCER 3
#define TOTAL_MESSAGES (PRODUCERS * MSGS_PER_PRODUCER)
#define QUEUE_CAPACITY TOTAL_MESSAGES
#define MAX_MSG_SIZE 64

 typedef struct {
   int queue_id;
   int producer_id;
   int messages_to_send;
 } ProducerArgs;
 
 typedef struct {
   int queue_id;
   int consumer_id;
   int messages_to_receive;
 } ConsumerArgs;

 static void dump_queue_state(const char* phase, int queue_id) {
  printf("\n===== %s =====\n", phase);
  MessageQueue_print_status(queue_id);
  MessageQueue_print_messages(queue_id);
  disastrOS_printStatus();
  printf("========================\n\n");
 }
 


static void producerProcess(void* args) {
  ProducerArgs* p_args = (ProducerArgs*) args;
  for (int i = 0; i < p_args->messages_to_send; ++i) {
    char payload[MAX_MSG_SIZE];
    snprintf(payload,
             sizeof(payload),
             "msg[p%d-%d] from pid=%d",
            p_args->producer_id,
             i,
             disastrOS_getpid());

    int ret = disastrOS_mq_send(p_args->queue_id, payload, (int) strlen(payload) + 1);
    printf("[PRODUCER %d | pid=%d] send #%d -> ret=%d, payload='%s'\n",
           p_args->producer_id,
           disastrOS_getpid(),
           i,
           ret,
           payload);

    MessageQueue_print_status(p_args->queue_id);
    disastrOS_preempt();
   }
 }
 
static void consumerProcess(void* args) {
  ConsumerArgs* c_args = (ConsumerArgs*) args;
  for (int i = 0; i < c_args->messages_to_receive; ++i) {
    char buffer[MAX_MSG_SIZE];
    memset(buffer, 0, sizeof(buffer));
 
    int ret = disastrOS_mq_receive(c_args->queue_id, buffer, sizeof(buffer));
    printf("[CONSUMER %d | pid=%d] recv #%d -> ret=%d, payload='%s'\n",
           c_args->consumer_id,
           disastrOS_getpid(),
           i,
           ret,
           ret >= 0 ? buffer : "");
    MessageQueue_print_status(c_args->queue_id);
    disastrOS_preempt();
   }
 
  disastrOS_exit(0);
}

static void wait_children(const char* phase, int queue_id, int children) {
  int left = children;
  while (left > 0) {
    int retval = 0;
    int pid = disastrOS_wait(0, &retval);
    if (pid < 0) {
      printf("[INIT] wait failed in phase '%s', left=%d\n", phase, left);
      return;
    }

    printf("[INIT][%s] child pid=%d terminated with retval=%d, left=%d\n",
           phase,
           pid,
           retval,
           left - 1);
    dump_queue_state("STATO DOPO TERMINAZIONE FIGLIO", queue_id);
    --left;
  }
}
 
static void initFunction(void* args) {
  (void) args;
 
  printf("[INIT] pid=%d, avvio test completo message queue\n", disastrOS_getpid());
 
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
 
  static ProducerArgs producer_args[PRODUCERS];
for (int i = 0; i < PRODUCERS; ++i) {
  producer_args[i].queue_id = qid;
  producer_args[i].producer_id = i;
  producer_args[i].messages_to_send = MSGS_PER_PRODUCER;
  disastrOS_spawn(producerProcess, &producer_args[i]);
  printf("[INIT] spawn producer[%d]\n", i);
}

wait_children("PRODUCER", qid, PRODUCERS);
dump_queue_state("QUEUE PIENA DI MESSAGGI PRODOTTI", qid);

static ConsumerArgs consumer_args[CONSUMERS];
int base = TOTAL_MESSAGES / CONSUMERS;
int remainder = TOTAL_MESSAGES % CONSUMERS;

for (int i = 0; i < CONSUMERS; ++i) {
  consumer_args[i].queue_id = qid;
  consumer_args[i].consumer_id = i;
  consumer_args[i].messages_to_receive = base + (i < remainder ? 1 : 0);
  disastrOS_spawn(consumerProcess, &consumer_args[i]);
  printf("[INIT] spawn consumer[%d], target_msgs=%d\n", i, consumer_args[i].messages_to_receive);
}

wait_children("CONSUMER", qid, CONSUMERS);
dump_queue_state("QUEUE DOPO CONSUMO", qid);

  int children_left = PRODUCERS + CONSUMERS;
  while (children_left > 0) {
    int retval = 0;
    int pid = disastrOS_wait(0, &retval);
    if (pid < 0) {
      printf("[INIT] wait failed while children_left=%d\n", children_left);
      break;
    }

    printf("[INIT] child pid=%d terminated with retval=%d, left=%d\n",
           pid,
           retval,
           children_left - 1);
    dump_queue_state("STATO DOPO TERMINAZIONE FIGLIO", qid);
    --children_left;
   }
 
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
 