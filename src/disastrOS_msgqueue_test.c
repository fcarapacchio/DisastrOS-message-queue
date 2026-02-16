#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "disastrOS.h"
#include "disastrOS_constants.h"
#include "disastrOS_msgqueuetest.h"
#define MQ_TEST_PACE_US 120000

static void mq_test_pace() {
  fflush(stdout);
  usleep(MQ_TEST_PACE_US);
}

static int test_failures = 0;
static int blocking_sender_qid = -1;

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
  mq_test_pace();
}

void MsgQueueTest_create_destroy() {
  printf("\n=== MsgQueueTest_create_destroy ===\n");
  const int qid = 100;

  printf("Creating queue %d\n", qid);
  mq_test_pace();
  if (disastrOS_mq_create(qid, 4) != qid) {
    printf("Failed to create queue %d\n", qid);
    test_failures++;
    mq_test_pace();
    return;
  }

  printf("Creating queue %d again (must fail)\n", qid);
  if (disastrOS_mq_create(qid, 4) != DSOS_EMQEXISTS) {
    printf("Queue %d was created twice\n", qid);
    test_failures++;
    mq_test_pace();
    return;
  }

  printf("Destroying queue %d\n", qid);
  if (disastrOS_mq_destroy(qid) != qid) {
    printf("Failed to destroy queue %d\n", qid);
    test_failures++;
    mq_test_pace();
    return;
  }

  printf("[MQ-TEST] create/destroy passed\n");
  mq_test_pace();
}

void MsgQueueTest_send() {
  printf("\n=== MsgQueueTest_send ===\n");
  const int qid = 101;
  char payload[] = "hello-queue";

  printf("Creating queue %d\n", qid);
  mq_test_pace();
  if (disastrOS_mq_create(qid, 2) != qid) {
    printf("Failed to create queue %d for send test\n", qid);
    test_failures++;
    mq_test_pace();
    return;
  }

  printf("Sending one message\n");
  if (disastrOS_mq_send(qid, payload, sizeof(payload)) != 0) {
    printf("Failed to send on queue %d\n", qid);
    test_failures++;
    mq_test_pace();
    return;
  }

  printf("Destroying queue %d\n", qid);
  if (disastrOS_mq_destroy(qid) != qid) {
    printf("Failed to destroy queue %d after send test\n", qid);
    test_failures++;
    mq_test_pace();
    return;
  }

  printf("[MQ-TEST] send passed\n");
}

void MsgQueueTest_receive() {
  printf("\n=== MsgQueueTest_receive ===\n");
  const int qid = 102;
  char payload[] = "recv-ok";
  char buffer[64];

  printf("Creating queue %d\n", qid);
  mq_test_pace();
  if (disastrOS_mq_create(qid, 4) != qid) {
    printf("Failed to create queue %d for receive test\n", qid);
    test_failures++;
    mq_test_pace();
    return;
  }

  printf("Sending payload before receive\n");
  if (disastrOS_mq_send(qid, payload, sizeof(payload)) != 0) {
    printf("Failed to pre-send payload on queue %d\n", qid);
    test_failures++;
    mq_test_pace();
    return;
  }

  memset(buffer, 0, sizeof(buffer));
  printf("Receiving payload\n");
  mq_test_pace();
  int received = disastrOS_mq_receive(qid, buffer, sizeof(buffer));
  if (received != (int) sizeof(payload)) {
    printf("Receive size mismatch, got %d expected %zu\n", received, sizeof(payload));
    test_failures++;
    mq_test_pace();
    return;
  }

  if (strcmp(buffer, payload) != 0) {
    printf("Payload mismatch: got '%s' expected '%s'\n", buffer, payload);
    test_failures++;
    mq_test_pace();
    return;
  }

  printf("Destroying queue %d\n", qid);
  mq_test_pace();
  if (disastrOS_mq_destroy(qid) != qid) {
    printf("Failed to destroy queue %d after receive test\n", qid);
    test_failures++;
    mq_test_pace();
    return;
  }

  printf("[MQ-TEST] receive passed\n");
}

void MsgQueueTest_blocking_behavior() {
  printf("\n=== MsgQueueTest_blocking_behavior (spawn) ===\n");
  const int qid = 103;
  char first_msg[] = "first";
  char out[32];

  printf("Creating queue %d with capacity 2\n", qid);
  mq_test_pace();
  if (disastrOS_mq_create(qid, 2) != qid) {
    printf("Failed to create queue %d for blocking test\n", qid);
    test_failures++;
    mq_test_pace();
    return;
  }

  printf("Sending first message\n");
  if (disastrOS_mq_send(qid, first_msg, sizeof(first_msg)) != 0) {
    printf("Failed to send first message\n");
    test_failures++;
    mq_test_pace();
    return;
  }

  printf("Spawning blocking sender\n");
  mq_test_pace();
  blocking_sender_qid = qid;
  disastrOS_spawn(blocking_sender, (void*) &blocking_sender_qid);

  // schedule child so it can enqueue its message
  disastrOS_preempt();
  memset(out, 0, sizeof(out));
  if (disastrOS_mq_receive(qid, out, sizeof(out)) <= 0) {
    printf("Failed first receive\n");
    test_failures++;
    mq_test_pace();
    return;
  }

  memset(out, 0, sizeof(out));
  if (disastrOS_mq_receive(qid, out, sizeof(out)) <= 0) {
    printf("Failed second receive\n");
    test_failures++;
    mq_test_pace();
    return;
  }

  int child_ret = 0;
 if (disastrOS_wait(0, &child_ret) < 0) {
    printf("Failed wait on spawned sender\n");
    test_failures++;
    mq_test_pace();
    return;
  }

  printf("Destroying queue %d\n", qid);
  mq_test_pace();
  if (disastrOS_mq_destroy(qid) != qid) {
    printf("Failed to destroy queue %d after blocking test\n", qid);
    test_failures++;
    mq_test_pace();
    return;
  }

  printf("[MQ-TEST] blocking behavior passed\n");
}

void MsgQueueTest_error_cases() {
  printf("\n=== MsgQueueTest_error_cases ===\n");
  const int qid = 104;
  char payload[] = "err-case";
  char buffer[32];

  printf("Trying to create queue with invalid size 0\n");
  mq_test_pace();
  if (disastrOS_mq_create(qid, 0) != DSOS_EMQINVALID) {
    printf("Queue created with invalid size 0\n");
    test_failures++;
    mq_test_pace();
    return;
  }

  printf("Trying to create queue with size > MAX_MESSAGES_PER_QUEUE\n");
  mq_test_pace();
  if (disastrOS_mq_create(qid, MAX_MESSAGES_PER_QUEUE + 1) != DSOS_EMQINVALID) {
    printf("Queue created with oversized capacity\n");
    test_failures++;
    mq_test_pace();
    return;
  }

  printf("Using invalid queue id on send/receive/destroy\n");
  mq_test_pace();
  if (disastrOS_mq_send(9999, payload, sizeof(payload)) != DSOS_EMQINVALID) {
    printf("Send succeeded on invalid queue id\n");
    test_failures++;
    mq_test_pace();
    return;
  }

  if (disastrOS_mq_receive(9999, buffer, sizeof(buffer)) != DSOS_EMQINVALID) {
    printf("Receive succeeded on invalid queue id\n");
    test_failures++;
    mq_test_pace();
    return;
  }

  if (disastrOS_mq_destroy(9999) != DSOS_EMQINVALID) {
    printf("Destroy succeeded on invalid queue id\n");
    test_failures++;
    mq_test_pace();
    return;
  }

  printf("Creating queue %d for buffer checks\n", qid);
  mq_test_pace();
  if (disastrOS_mq_create(qid, 2) != qid) {
    printf("Failed creating queue %d for buffer checks\n", qid);
    test_failures++;
    mq_test_pace();
    return;
  }

  printf("Trying to send with zero-size buffer\n");
  mq_test_pace();
  if (disastrOS_mq_send(qid, payload, 0) != DSOS_EBUFFER) {
    printf("Send with size 0 did not fail as expected\n");
    test_failures++;
    mq_test_pace();
    return;
  }

  printf("Trying to receive with zero-size buffer\n");
  mq_test_pace();
  if (disastrOS_mq_receive(qid, buffer, 0) != DSOS_EBUFFER) {
    printf("Receive with size 0 did not fail as expected\n");
    test_failures++;
    mq_test_pace();
    return;
  }

  printf("Destroying queue %d\n", qid);
  mq_test_pace();
  if (disastrOS_mq_destroy(qid) != qid) {
    printf("Failed destroying queue %d after error checks\n", qid);
    test_failures++;
    mq_test_pace();
    return;
  }

  printf("[MQ-TEST] error cases passed\n");
}


void MsgQueueTest_runAll() {
  MsgQueueTest_init();

  MsgQueueTest_create_destroy();
  disastrOS_printStatus();

  MsgQueueTest_send();
  disastrOS_printStatus();

  MsgQueueTest_receive();
  disastrOS_printStatus();

  MsgQueueTest_blocking_behavior();
  disastrOS_printStatus();

  MsgQueueTest_error_cases();
  disastrOS_printStatus();

  if (!test_failures)
    printf("\n[MQ-TEST] ALL TESTS PASSED\n");
  else
    printf("\n[MQ-TEST] FAILURES=%d\n", test_failures);

 }