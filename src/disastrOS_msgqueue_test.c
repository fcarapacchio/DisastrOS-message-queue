#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "disastrOS.h"
#include "disastrOS_constants.h"
#include "disastrOS_msgqueuetest.h"
#include "disastrOS_message_queue.h"

static int test_failures = 0;
static int blocking_sender_qid = -1;

static void blocking_sender(void* args) {
  int qid = *(int*)args;
  char third_msg[] = "third";
  int ret = disastrOS_mq_send(qid, third_msg, sizeof(third_msg));
  printf("[MQ-TEST] Message Queue after blocking sender:\n");
  MessageQueue_print(qid);
  printf("[MQ-TEST] blocking sender send ret=%d\n", ret);
  disastrOS_exit(ret);
}

static void blocking_receiver(void* args) {
  int qid = *(int*) args;
  char in[32];
  memset(in, 0, sizeof(in));
  int ret = disastrOS_mq_receive(qid, in, sizeof(in));
  printf("[MQ-TEST] blocking receiver receive ret=%d payload='%s'\n", ret, in);
  disastrOS_exit(ret);
}

void MsgQueueTest_function() {
  printf("\n=== MsgQueueTest_function ===\n");
  const int qid = 100;
  char payload[] = "hello-queue";
  char buffer[64];

  printf("\nCreating queue %d\n", qid);
  if (disastrOS_mq_create(qid, 4) != qid) {
    printf("Failed to create queue %d for receive test\n", qid);
    test_failures++;
    mq_test_pace();
    return;
  }
  printf("Queue %d created successfully!\n", qid);
  MessageQueue_print(qid);

  printf("Creating queue %d again (must fail)\n", qid);
  if (disastrOS_mq_create(qid, 4) != DSOS_EMQEXISTS) {
    printf("Queue %d was created twice\n", qid);
    test_failures++;
    mq_test_pace();
    return;
  }
  printf("Queue %d already exists, can't create another with same queue id\n", qid);

  printf("Sending one message\n");
  if (disastrOS_mq_send(qid, payload, sizeof(payload)) != 0) {
    printf("Failed to send message on queue %d\n", qid);
    test_failures++;
    mq_test_pace();
    return;
  }

  printf("Message sent successfully!\n");

  memset(buffer, 0, sizeof(buffer));
  printf("Receiving message\n");
  int received = disastrOS_mq_receive(qid, buffer, sizeof(buffer));
  if (received != (int) sizeof(payload)) {
    printf("Receive size mismatch, got %d expected %zu\n", received, sizeof(payload));
    test_failures++;
    mq_test_pace();
    return;
  }

  printf("Message received successfully!\n");

  if (strcmp(buffer, payload) != 0) {
    printf("Payload mismatch: got '%s' expected '%s'\n", buffer, payload);
    test_failures++;
    mq_test_pace();
    return;
  }

  printf("Destroying queue %d\n", qid);
  if (disastrOS_mq_destroy(qid) != qid) {
    printf("Failed to destroy queue %d after receive test\n", qid);
    test_failures++;
    mq_test_pace();
    return;
  }

  printf("Queue %d destroyed successfully!\n", qid);

  printf("[MQ-TEST] message queue test passed\n");
}

void MsgQueueTest_blocking_sender() {
  printf("\n=== MsgQueueTest_blocking_sender (spawn) ===\n");
  const int qid = 101;
  char first_msg[] = "first";
  char second_msg[] = "second";
  char out[32];

  printf("\nCreating queue %d with capacity 2\n", qid);
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
  
  printf("Sending second message (queue becomes full)\n");
  if (disastrOS_mq_send(qid, second_msg, sizeof(second_msg)) != 0) {
    printf("Failed to send second message\n");
    test_failures++;
    mq_test_pace();
    return;
  }


  printf("Spawning blocking sender\n");
  blocking_sender_qid = qid;
  disastrOS_spawn(blocking_sender, (void*) &blocking_sender_qid);

  // schedule child so it can enqueue its message
  disastrOS_preempt();
  memset(out, 0, sizeof(out));
  printf("Receiving a message to allow the sending of another message\n");
  if (disastrOS_mq_receive(qid, out, sizeof(out)) <= 0) {
    printf("Failed first receive\n");
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
  if (disastrOS_mq_destroy(qid) != qid) {
    printf("Failed to destroy queue %d after blocking sender test\n", qid);
    test_failures++;
    mq_test_pace();
    return;
  }

  printf("[MQ-TEST] blocking sender passed\n");
}

void MsgQueueTest_blocking_receiver() {
  printf("\n=== MsgQueueTest_blocking_receiver (spawn) ===\n");
  const int qid = 102;
  char wake_msg[] = "wake-receiver";

  printf("\nCreating queue %d with capacity 2\n", qid);  
  if (disastrOS_mq_create(qid, 2) != qid) {
    printf("Failed to create queue %d for blocking receiver test\n", qid);
    test_failures++;
    mq_test_pace();
    return;
  }

  printf("Spawning receiver on empty queue\n");
  int receiver_qid = qid;
  disastrOS_spawn(blocking_receiver, (void*) &receiver_qid);

  // let child run: it should attempt receive and block on waiting_receivers
  disastrOS_preempt();

  printf("Sending one message to wake blocked receiver\n");

  if (disastrOS_mq_send(qid, wake_msg, sizeof(wake_msg)) != 0) {
    printf("Failed to send wake message on queue %d\n", qid);
    test_failures++;
    mq_test_pace();
    return;
  }
  
  printf("[MQ-TEST] Message queue after blocking receiver\n");
  MessageQueue_print(qid);

  // let receiver run and consume message
  disastrOS_preempt();

  int child_ret = 0;
  if (disastrOS_wait(0, &child_ret) < 0) {
    printf("Failed wait on blocking receiver child\n");
    test_failures++;
    mq_test_pace();
    return;
  }

  if (child_ret < 0) {
    printf("Blocking receiver child returned error %d\n", child_ret);
    test_failures++;
    mq_test_pace();
    return;
  }

  printf("Destroying queue %d\n", qid);
  if (disastrOS_mq_destroy(qid) != qid) {
    printf("Failed to destroy queue %d after blocking receiver test\n", qid);
    test_failures++;
    mq_test_pace();
    return;
  }
  

  printf("[MQ-TEST] blocking receiver passed\n");
}

void MsgQueueTest_error_cases() {
  printf("\n=== MsgQueueTest_error_cases ===\n");
  const int qid = 104;
  char payload[] = "err-case";
  char buffer[32];

  printf("Trying to create queue with invalid size 0\n");
  if (disastrOS_mq_create(qid, 0) != DSOS_EMQINVALID) {
    printf("Queue created with invalid size 0\n");
    test_failures++;
    mq_test_pace();
    return;
  }


  printf("Trying to create queue with size > MAX_MESSAGES_PER_QUEUE\n");
  if (disastrOS_mq_create(qid, MAX_MESSAGES_PER_QUEUE + 1) != DSOS_EMQINVALID) {
    printf("Queue created with oversized capacity\n");
    test_failures++;
    mq_test_pace();
    return;
  }

  printf("Using invalid queue id on send/receive/destroy\n");
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
  if (disastrOS_mq_create(qid, 2) != qid) {
    printf("Failed creating queue %d for buffer checks\n", qid);
    test_failures++;
    mq_test_pace();
    return;
  }

  printf("Trying to send with zero-size buffer\n");
  if (disastrOS_mq_send(qid, payload, 0) != DSOS_EBUFFER) {
    printf("Send with size 0 did not fail as expected\n");
    test_failures++;
    mq_test_pace();
    return;
  }

  printf("Trying to receive with zero-size buffer\n");
  if (disastrOS_mq_receive(qid, buffer, 0) != DSOS_EBUFFER) {
    printf("Receive with size 0 did not fail as expected\n");
    test_failures++;
    mq_test_pace();
    return;
  }

  printf("Destroying queue %d\n", qid);
  if (disastrOS_mq_destroy(qid) != qid) {
    printf("Failed destroying queue %d after error checks\n", qid);
    test_failures++;
    mq_test_pace();
    return;
  }

  printf("[MQ-TEST] error cases passed\n");
}


void MsgQueueTest_runAll() {

  MsgQueueTest_function();

  MsgQueueTest_blocking_sender();

  MsgQueueTest_blocking_receiver();

  MsgQueueTest_error_cases();

  if (!test_failures)
    printf("\n[MQ-TEST] ALL TESTS PASSED\n\n");
  else
    printf("\n[MQ-TEST] FAILURES=%d\n\n", test_failures);

 }