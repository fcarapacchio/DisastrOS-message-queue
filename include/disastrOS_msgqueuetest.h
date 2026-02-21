#pragma once

// Test message queue: create, send, receive and destroy
void MsgQueueTest_function();

// Test blocking behavior when queue is full
void MsgQueueTest_blocking_sender();

// Test blocking behavior when queue is empty
void MsgQueueTest_blocking_receiver();

// Test error and edge cases
void MsgQueueTest_error_cases();

// Debug function to run all MQ tests
void MsgQueueTest_runAll();
