#include <stdio.h>
#include <string.h>

#include "disastrOS.h"
#include "disastrOS_constants.h"
#include "disastrOS_msgqueuetest.h"

static int test_failures = 0;
static int blocking_sender_qid = -1;

static void check(int condition, const char* name) {
  if (condition) {
    printf("[MQ-TEST][PASS] %s\n", name);
  } else {
    printf("[MQ-TEST][FAIL] %s\n", name);
    test_failures++;
  }
}

static void blocking_sender(void* args) {
  int qid = *(int*)args;
  char second_msg[] = "second";
  int ret = disastrOS_mq_send(qid, second_msg, sizeof(second_msg));
  printf("[MQ-TEST] blocking sender send ret=%d\n", ret);
  disastrOS_exit(ret);
}

void MsgQueueTest_init() {
  test_failures = 0;
  printf("\n=== MsgQueueTest_init ===\n");
}

void MsgQueueTest_create_destroy() {
  printf("\n=== MsgQueueTest_create_destroy ===\n");
  const int qid = 100;

  int ret = disastrOS_mq_create(qid, 4);
  check(ret == qid, "mq_create returns queue id");

  ret = disastrOS_mq_create(qid, 4);
  check(ret == DSOS_EMQEXISTS, "mq_create on existing queue returns DSOS_EMQEXISTS");

  ret = disastrOS_mq_destroy(qid);
  check(ret == qid, "mq_destroy returns queue id");
}

void MsgQueueTest_send() {
  printf("\n=== MsgQueueTest_send ===\n");
  const int qid = 101;
  char payload[] = "hello-queue";

  int ret = disastrOS_mq_create(qid, 2);
  check(ret == qid, "mq_create for send test");

  ret = disastrOS_mq_send(qid, payload, sizeof(payload));
  check(ret == 0, "mq_send success returns 0");

  ret = disastrOS_mq_destroy(qid);
  check(ret == qid, "mq_destroy after send test");
}

void MsgQueueTest_receive() {
  printf("\n=== MsgQueueTest_receive ===\n");
  const int qid = 102;
  char payload[] = "recv-ok";
  char buffer[64];

  int ret = disastrOS_mq_create(qid, 4);
  check(ret == qid, "mq_create for receive test");

  ret = disastrOS_mq_send(qid, payload, sizeof(payload));
  check(ret == 0, "mq_send before receive");

  memset(buffer, 0, sizeof(buffer));
  ret = disastrOS_mq_receive(qid, buffer, sizeof(buffer));
  check(ret == (int)sizeof(payload), "mq_receive returns payload size");
  check(strcmp(buffer, payload) == 0, "mq_receive payload matches");

  ret = disastrOS_mq_destroy(qid);
  check(ret == qid, "mq_destroy after receive test");
}

void MsgQueueTest_blocking_behavior() {
  printf("\n=== MsgQueueTest_blocking_behavior (spawn) ===\n");
  const int qid = 103;
  char first_msg[] = "first";
  char out[32];

  int ret = disastrOS_mq_create(qid, 2);
  check(ret == qid, "mq_create for spawn sender test");

  ret = disastrOS_mq_send(qid, first_msg, sizeof(first_msg));
   check(ret == 0, "initial send");
  blocking_sender_qid = qid;
  disastrOS_spawn(blocking_sender, (void*)&blocking_sender_qid);
  check(1, "spawn sender");

  // schedule child so it can enqueue its message
  disastrOS_preempt();
  memset(out, 0, sizeof(out));
  ret = disastrOS_mq_receive(qid, out, sizeof(out));

  check(ret > 0, "first receive");

  memset(out, 0, sizeof(out));
  ret = disastrOS_mq_receive(qid, out, sizeof(out));
  check(ret > 0, "second receive");

  int child_ret = 0;
  int waited = disastrOS_wait(0, &child_ret);
  check(waited >= 0, "wait on spawned sender child");

  ret = disastrOS_mq_destroy(qid);
  check(ret == qid, "mq_destroy after spawn sender test");
}

void MsgQueueTest_runAll() {
  MsgQueueTest_init();
  MsgQueueTest_create_destroy();
  MsgQueueTest_send();
  MsgQueueTest_receive();
  MsgQueueTest_blocking_behavior();

  if (!test_failures)
    printf("\n[MQ-TEST] ALL TESTS PASSED\n");
  else
    printf("\n[MQ-TEST] FAILURES=%d\n", test_failures);
}