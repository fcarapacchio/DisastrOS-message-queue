#pragma once

// Initializes the test suite for message queues
void MsgQueueTest_init();

// Test creating and destroying message queues
void MsgQueueTest_create_destroy();

// Test sending messages
void MsgQueueTest_send();

// Test receiving messages
void MsgQueueTest_receive();

// Test blocking behavior when queue is full/empty
void MsgQueueTest_blocking_behavior();

// Test error and edge cases
void MsgQueueTest_error_cases();

// Debug function to run all MQ tests
void MsgQueueTest_runAll();
