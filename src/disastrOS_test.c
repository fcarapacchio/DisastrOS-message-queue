#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "disastrOS.h"
#include "disastrOS_message_queue.h"
#include "disastrOS_msgqueuetest.h"

// using two message queues for this test, small queue capacity to force blocking behavior
#define Q1_ID 200
#define Q2_ID 201
#define QUEUE_CAPACITY 2
#define MAX_MSG_SIZE 64
#define MAX_ACTIONS 100

// arguments passed to producer (sender) processes
typedef struct {
  int queue_id;  // which queue to send to
  int seq;    // message sequence number (used for the payload)
} ProducerArgs;


// arguments passed to consumer (receiver) processes
typedef struct {
  int queue_id;  // which queue to receive from
  int expected_seq;  // expected message number
} ConsumerArgs;


// possible action types in the test scenario
typedef enum {
  ACTION_SPAWN_PRODUCER,
  ACTION_SPAWN_CONSUMER,
  ACTION_PREEMPT
} ActionType;

typedef struct {
  ActionType type;  // what to do
  int queue_id;
  int value;
} TestAction;

// we need this to handle the sleep state
void sleeperFunction(void* args){
  printf("Hello, I am the sleeper, and I sleep %d\n",disastrOS_getpid());
  while(1) {
    getc(stdin);
    disastrOS_printStatus();
  }
}


// producer process: sends a message to the specified queue
static void producer(void* args) {
  ProducerArgs* p = (ProducerArgs*) args;
  char payload[MAX_MSG_SIZE];
  snprintf(payload, sizeof(payload), "msg-%d", p->seq);  // build the message using seq parameter

  disastrOS_printStatus();
  
  int ret = disastrOS_mq_send(p->queue_id, payload, (int) strlen(payload) + 1);
  if (ret < 0) {
    printf("[PRODUCER] send failed q=%d seq=%d ret=%d\n", p->queue_id, p->seq, ret);
    disastrOS_exit(ret);
    return;
  }
  disastrOS_exit(0);
}


// consumer process: receives a message from the specified queue and verifies that is the expected one
static void consumer(void* args) {
  ConsumerArgs* c = (ConsumerArgs*) args;
  char payload[MAX_MSG_SIZE];
  char expected[MAX_MSG_SIZE];

  disastrOS_printStatus();

  memset(payload, 0, sizeof(payload));
  int ret = disastrOS_mq_receive(c->queue_id, payload, sizeof(payload));
  if (ret <= 0) {
    printf("[CONSUMER] receive failed q=%d ret=%d\n", c->queue_id, ret);
    disastrOS_exit(ret);
    return;
  }

  snprintf(expected, sizeof(expected), "msg-%d", c->expected_seq);  // checks message correctness
  if (strcmp(payload, expected) != 0) {
    printf("[CONSUMER] mismatch q=%d got='%s' expected='%s'\n", c->queue_id, payload, expected);
    disastrOS_exit(-1);
    return;
  }

  disastrOS_exit(0);
}


// wait for n children to terminate and checks that all exit successfully
static int wait_n_ok(int n, const char* phase) {
  for (int i = 0; i < n; ++i) {
    int retval = 0;
    int pid = disastrOS_wait(0, &retval);
    if (pid < 0 || retval < 0) {
      printf("[INIT][%s] failed (pid=%d retval=%d)\n", phase, pid, retval);
      return -1;
    }
  }
  return 0;
}


// executes a sequence of TestAction objects: each action may spawn a producer, spawn a concumer or force a preemption
static int run_actions(const TestAction* actions, int count, const char* phase) {
  
  // arrays used to store arguments passed to spawned processes
  static ProducerArgs producer_args_global[MAX_ACTIONS];
static ConsumerArgs consumer_args_global[MAX_ACTIONS];
  int spawned = 0;
  int prod_count = 0;
  int cons_count = 0;

  for (int i = 0; i < count; ++i) {
    if (actions[i].type == ACTION_PREEMPT) {
      disastrOS_preempt();
      disastrOS_printStatus();
      continue;
    }

    if (actions[i].type == ACTION_SPAWN_PRODUCER) {
      producer_args_global[prod_count].queue_id = actions[i].queue_id;
      producer_args_global[prod_count].seq = actions[i].value;
      disastrOS_spawn(producer, &producer_args_global[prod_count]);
      disastrOS_printStatus();
      prod_count++;
      spawned++;
      continue;
}

    if (actions[i].type == ACTION_SPAWN_CONSUMER) {
      consumer_args_global[cons_count].queue_id = actions[i].queue_id;
      consumer_args_global[cons_count].expected_seq = actions[i].value;
      disastrOS_spawn(consumer, &consumer_args_global[cons_count]);
      disastrOS_printStatus();
      cons_count++;
      spawned++;
    }
}
  
  // wait for all spawned processes
  return wait_n_ok(spawned, phase);
}

/*
  Integration test scenario:

     Tests:
       - blocking receive (consumer first)
       - blocking send (queue full)
       - correct wakeup behavior
       - multiple independent queues
*/
static int run_mq_scenario(void) {
  printf("[TEST] MQ integration scenario\n");

  // create two message queues
  if (disastrOS_mq_create(Q1_ID, QUEUE_CAPACITY) != Q1_ID)
    return -1;
  if (disastrOS_mq_create(Q2_ID, QUEUE_CAPACITY) != Q2_ID) {
    disastrOS_mq_destroy(Q1_ID);
    return -1;
  }
  disastrOS_printStatus();

  const TestAction phase1[] = {
    {ACTION_SPAWN_CONSUMER, Q1_ID, 1},
    {ACTION_PREEMPT, 0, 0},
    {ACTION_SPAWN_PRODUCER, Q1_ID, 1}
  };

  const TestAction phase2[] = {
    {ACTION_SPAWN_PRODUCER, Q2_ID, 10},
    {ACTION_SPAWN_PRODUCER, Q2_ID, 11}
  };

  const TestAction phase3[] = {
    {ACTION_SPAWN_PRODUCER, Q2_ID, 12},
    {ACTION_PREEMPT, 0, 0},
    {ACTION_SPAWN_CONSUMER, Q2_ID, 10},
    {ACTION_SPAWN_CONSUMER, Q2_ID, 11},
    {ACTION_SPAWN_CONSUMER, Q2_ID, 12}
  };

  const TestAction phase4[] = {
    {ACTION_SPAWN_PRODUCER, Q1_ID, 2},
    {ACTION_SPAWN_PRODUCER, Q2_ID, 20},
    {ACTION_SPAWN_CONSUMER, Q1_ID, 2},
    {ACTION_SPAWN_CONSUMER, Q2_ID, 20}
  };


  // execute all 4 phases
  if (run_actions(phase1, 3, "PHASE_1") < 0) goto fail;
  if (run_actions(phase2, 2, "PHASE_2") < 0) goto fail;
  if (run_actions(phase3, 5, "PHASE_3") < 0) goto fail;
  if (run_actions(phase4, 4, "PHASE_4") < 0) goto fail;

  // message queue cleanup
  if (disastrOS_mq_destroy(Q2_ID) != Q2_ID) goto fail;
  if (disastrOS_mq_destroy(Q1_ID) != Q1_ID) goto fail;


  printf("[TEST] MQ scenario PASSED\n");
  return 0;

fail:
  disastrOS_mq_destroy(Q2_ID);
  disastrOS_mq_destroy(Q1_ID);
  printf("[TEST] MQ scenario FAILED\n");
  return -1;
}

// initial process executed by the kernel
static void initFunction(void* args) {
  (void) args;
  int retval;

  // launch of a process to test message queues
  disastrOS_spawn(MsgQueueTest_runAll, 0);
  disastrOS_wait(0, &retval);

  disastrOS_spawn(sleeperFunction, 0);


  if (run_mq_scenario() < 0)
    printf("[INIT] integration test FAILED\n");
  else
    printf("[INIT] integration test PASSED\n");

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