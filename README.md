# DisastrOS Message Queue
An implementation of **blocking message queues** for disastrOS.
## Instructions

### Compile
```bash
make
```
### Run tests:
```bash
./disastrOS_test
```
to start the tests.

## Usage
A Message Queue is a structure that stores messages in FIFO order and supports blocking semantics for processes.
In this project, a message queue is implemented as a kernel resource shared between all processes.
## API
- `disastrOS_mq_create(int queue_id, int max_msgs)`
   - Creates a message queue with global (must be unique) id `queue_id` and capacity `max_msgs`.
   - Returns the queue id on success, otherwise an error code.
- `disastrOS_mq_send(int queue_id, void* msg, int size)`
   - Sends a message to the queue.
   - If the queue is full, the sender is blocked and added to the waiting_senders list.
- `disastrOS_mq_receive(int queue_id, void* msg_buffer, int size)`
   - Receives the first available message from the queue.
   - If the queue is empty, the receiver is blocked and added to the waiting_receivers list.
- `disastrOS_mq_destroy(int queue_id)`
   - Destroys the queue.
   - Wakes up all blocked processes.
   - Frees pending messages.

## Error codes
In include/disastrOS_constants.h are defined the following error codes:
- `DSOS_EMQFULL` — queue is full
- `DSOS_EMQEMPTY` — queue is empty
- `DSOS_EMQINVALID` — invalid queue id/params
- `DSOS_EMQTIMEOUT` — timeout in receive (reserved)
- `DSOS_EERROR` — generic error
- `DSOS_EMQCREATE` — queue creation failure
- `DSOS_EMQEXISTS` — queue already exists
- `DSOS_EMQCLOSED` — queue already closed
- `DSOS_EMQNOTOPEN` — queue not open
- `DSOS_EBUFFER` — invalid buffer size
