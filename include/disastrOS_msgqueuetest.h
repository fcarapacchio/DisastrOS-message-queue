#pragma once

// Test creating and destroying message queues
void MsgQueueTest_create_destroy();

// Test sending messages
void MsgQueueTest_send();

// Test receiving messages
void MsgQueueTest_receive();

// Test blocking behavior when queue is full
void MsgQueueTest_blocking_sender();

// Test blocking behavior when queue is empty
void MsgQueueTest_blocking_receiver();

// Test error and edge cases
void MsgQueueTest_error_cases();

// Debug function to run all MQ tests
void MsgQueueTest_runAll();
