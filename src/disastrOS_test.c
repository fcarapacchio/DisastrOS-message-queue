#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>

#include "disastrOS.h"
#include "disastrOS_msgqueuetest.h"
#include "disastrOS_message_queue.h"

#define TEST_QUEUE_ID 200
#define NUM_CHILDREN 10

typedef struct {
  int queue_id;
  int producer_index;
} ProducerArgs;

typedef struct {
  int queue_id;
  int receiver_index;
} ReceiverArgs;

// dedicated producer used to send real messages in the runtime queue
void mqProducerFunction(void* args){
  ProducerArgs* producer_args=(ProducerArgs*) args;
  char msg[64];
  snprintf(msg, sizeof(msg), "producer[%d] pid=%d", producer_args->producer_index, disastrOS_getpid());
  int send_ret=disastrOS_mq_send(producer_args->queue_id, msg, (int) strlen(msg)+1);
  printf("producer pid=%d, mq_send ret=%d, msg='%s'\n", disastrOS_getpid(), send_ret, msg);
  disastrOS_exit(send_ret);
}

void mqReceiverFunction(void* args){
  ReceiverArgs* receiver_args=(ReceiverArgs*) args;
  char msg[64];
  memset(msg, 0, sizeof(msg));
  int recv_ret=disastrOS_mq_receive(receiver_args->queue_id, msg, sizeof(msg));
  printf("receiver[%d] pid=%d, mq_receive ret=%d, msg='%s'\n",
         receiver_args->receiver_index,
         disastrOS_getpid(),
         recv_ret,
         recv_ret >= 0 ? msg : "");
  disastrOS_exit(recv_ret < 0 ? recv_ret : 0);
}

// we need this to handle the sleep state
void sleeperFunction(void* args){
  printf("Hello, I am the sleeper, and I sleep %d\n",disastrOS_getpid());
  while(1) {
    getc(stdin);
    disastrOS_printStatus();
  }
}

void childFunction(void* args){
  (void) args;
  printf("Hello, I am the child function %d\n",disastrOS_getpid());
  printf("I will iterate a bit, before terminating\n");
  char msg[64];
  snprintf(msg, sizeof(msg), "child pid=%d started", disastrOS_getpid());
  int send_ret=disastrOS_mq_send(TEST_QUEUE_ID, msg, (int) strlen(msg)+1);
  printf("child pid=%d, mq_send ret=%d, msg='%s'\n", disastrOS_getpid(), send_ret, msg);
  int type=0;
  int mode=0;
  int fd=disastrOS_openResource(disastrOS_getpid(),type,mode);
  printf("fd=%d\n", fd);
  printf("PID: %d, terminating\n", disastrOS_getpid());

  for (int i=0; i<3; ++i){
    printf("PID: %d, iterate %d\n", disastrOS_getpid(), i);
  }
  disastrOS_exit(disastrOS_getpid()+1);
}

void mq_test_process(void* args) {
  (void) args; 
  MsgQueueTest_runAll();
  disastrOS_exit(0);
  int mq_retval = 0;
  int waited_pid = disastrOS_wait(0, &mq_retval);
  if (waited_pid < 0)
    printf("failed to wait MQ test process\n");
  else
    printf("MQ test process pid=%d finished with retval=%d\n", waited_pid, mq_retval);
}


void initFunction(void* args) {
  (void) args;
  static ReceiverArgs waiting_receiver_args;
  static ProducerArgs waiting_sender_args;
  static ProducerArgs producer_args[NUM_CHILDREN]; 
  disastrOS_printStatus();
  printf("hello, I am init and I just started\n");
  

  int qid=disastrOS_mq_create(TEST_QUEUE_ID, NUM_CHILDREN);
  printf("create runtime queue id=%d\n", qid);
  if (qid<0) {
    printf("cannot create runtime queue, aborting integration part\n");
    printf("shutdown!");
    disastrOS_shutdown();
    return;
  }

  // --- integrated blocking scenario: waiting receiver + waiting sender ---
  waiting_receiver_args.queue_id=TEST_QUEUE_ID;
  waiting_receiver_args.receiver_index=0;
  disastrOS_spawn(mqReceiverFunction, &waiting_receiver_args);
  printf("after spawning receiver on empty queue:\n");
  MessageQueue_print_status(qid);

  char wake_msg[64];
  snprintf(wake_msg, sizeof(wake_msg), "wake-receiver-from-init");
  int wake_ret=disastrOS_mq_send(TEST_QUEUE_ID, wake_msg, (int) strlen(wake_msg)+1);
  printf("wake receiver send from init, ret=%d\n", wake_ret);

  int retval;
  int pid;
  for (int i=0; i<1; ++i) {
    pid=disastrOS_wait(0, &retval);
    printf("blocking receive phase, pid=%d retval=%d\n", pid, retval);
  }

  // fill queue to force waiting_senders
  printf("prefill phase starts: queue should become full\n");
  for (int i=0; i<NUM_CHILDREN; ++i) {
    char prefill_msg[64];
    snprintf(prefill_msg, sizeof(prefill_msg), "prefill[%d]", i);
    int send_ret=disastrOS_mq_send(TEST_QUEUE_ID, prefill_msg, (int) strlen(prefill_msg)+1);
    printf("prefill send[%d], ret=%d\n", i, send_ret);
  }

  printf("after prefill, queue status before blocked sender spawn:\n");
  MessageQueue_print_status(qid);

  waiting_sender_args.queue_id=TEST_QUEUE_ID;
  waiting_sender_args.producer_index=101;
  disastrOS_spawn(mqProducerFunction, &waiting_sender_args);
  printf("after spawning sender on full queue (snapshot before child runs):\n");
  MessageQueue_print_status(qid);

  // free one slot and then wait sender termination
  char unblock_msg[64];
  memset(unblock_msg, 0, sizeof(unblock_msg));
  int unblock_recv=disastrOS_mq_receive(TEST_QUEUE_ID, unblock_msg, sizeof(unblock_msg));
  printf("unblock receive ret=%d msg='%s'\n", unblock_recv, unblock_msg);

  pid=disastrOS_wait(0, &retval);
  printf("blocking send phase, pid=%d retval=%d\n", pid, retval);

  // drain prefill queue to continue with previous scenario from clean state
  for (int i=0; i<NUM_CHILDREN; ++i) {
    char msg[64];
    memset(msg, 0, sizeof(msg));
    int recv_ret=disastrOS_mq_receive(TEST_QUEUE_ID, msg, sizeof(msg));
    printf("post-block drain[%d] ret=%d msg='%s'\n", i, recv_ret, msg);
  }

  // explicit send phase: producers send all messages first
  int alive_producers=0;
  for (int i=0; i<NUM_CHILDREN; ++i) {
    producer_args[i].queue_id=TEST_QUEUE_ID;
    producer_args[i].producer_index=i;
    disastrOS_spawn(mqProducerFunction, &producer_args[i]);
    alive_producers++;
  }

  int successful_messages=0;
  while (alive_producers>0 && (pid=disastrOS_wait(0, &retval))>=0){
    if (retval==0)
      successful_messages++;
    printf("producer pid=%d terminated, retval=%d, remaining=%d\n", pid, retval, alive_producers-1);
    --alive_producers;
  }

  printf("receiving %d messages from producers\n", successful_messages);
  for (int i=0; i<successful_messages; ++i) {
    char msg[64];
    memset(msg, 0, sizeof(msg));
    int recv_ret=disastrOS_mq_receive(TEST_QUEUE_ID, msg, sizeof(msg));
    printf("init mq_receive[%d] ret=%d msg='%s'\n", i, recv_ret, msg);
  }


  printf("I feel like to spawn 10 nice threads\n");
  int alive_children=0;
  for (int i=0; i<NUM_CHILDREN; ++i) {
    int type=0;
    int mode=DSOS_CREATE;
    printf("mode: %d\n", mode);
    printf("opening resource (and creating if necessary)\n");
    int fd=disastrOS_openResource(i,type,mode);
    printf("fd=%d\n", fd);
    disastrOS_spawn(childFunction, 0);
    alive_children++;
  }

  // Let every child run at least once so queue gets populated before receives.
  for (int i=0; i<NUM_CHILDREN; ++i) {
    disastrOS_preempt();
  }


  printf("receiving %d messages from children\n", NUM_CHILDREN);
  for (int i=0; i<NUM_CHILDREN; ++i) {
    char msg[64];
    memset(msg, 0, sizeof(msg));
    MessageQueue_print_status(qid);
    MessageQueue_print_messages(qid);
    int recv_ret=disastrOS_mq_receive(TEST_QUEUE_ID, msg, sizeof(msg));
    printf("init mq_receive[%d] ret=%d msg='%s'\n", i, recv_ret, msg);
  }

  disastrOS_printStatus();
  while(alive_children>0 && (pid=disastrOS_wait(0, &retval))>=0){
    disastrOS_printStatus();
    printf("initFunction, child: %d terminated, retval:%d, alive: %d \n",
           pid, retval, alive_children);
    --alive_children;
  }
  int destroy_ret=disastrOS_mq_destroy(TEST_QUEUE_ID);
  printf("destroy runtime queue ret=%d\n", destroy_ret);
  

  printf("shutdown!");
  disastrOS_shutdown();
}


int main(int argc, char** argv){
  char* logfilename=0;
  if (argc>1) {
    logfilename=argv[1];
  }
  // we create the init process processes
  // the first is in the running variable
  // the others are in the ready queue
  printf("the function pointer is: %p", childFunction);
  // spawn an init process
  printf("start\n");
  disastrOS_start(initFunction, 0, logfilename);
  return 0;
}