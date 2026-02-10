# Architecture & Design Documentation

## System Architecture

### Overview

The server uses a **fixed-size thread pool** architecture to handle multiple concurrent clients efficiently without the overhead of creating a new thread for each connection.

```
┌──────────────────────────────────────────────────────────────┐
│                         Main Thread                           │
│                                                                │
│  1. Initialize server socket                                  │
│  2. Bind to port                                              │
│  3. Listen for connections                                    │
│  4. Accept loop:                                              │
│     ┌─────────────────────────────────────────────────────┐  │
│     │  accept() → get client socket                        │  │
│     │  ↓                                                    │  │
│     │  Enqueue task to thread pool                         │  │
│     │  ↓                                                    │  │
│     │  Continue accepting more connections                 │  │
│     └─────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────┘
                              ↓
┌──────────────────────────────────────────────────────────────┐
│                        Thread Pool                            │
│                                                                │
│  ┌────────────────────────────────────────────────────────┐  │
│  │              Thread-Safe Task Queue                     │  │
│  │  - Protected by mutex                                   │  │
│  │  - Condition variable for synchronization              │  │
│  │  - FIFO ordering                                        │  │
│  └────────────────────────────────────────────────────────┘  │
│                              ↓                                │
│  ┌───────────┐  ┌───────────┐  ┌───────────┐  ┌──────────┐ │
│  │ Worker 1  │  │ Worker 2  │  │ Worker 3  │  │ Worker 4 │ │
│  │           │  │           │  │           │  │          │ │
│  │ • Wait    │  │ • Wait    │  │ • Wait    │  │ • Wait   │ │
│  │ • Dequeue │  │ • Dequeue │  │ • Dequeue │  │ • Dequeue│ │
│  │ • Process │  │ • Process │  │ • Process │  │ • Process│ │
│  │ • Repeat  │  │ • Repeat  │  │ • Repeat  │  │ • Repeat │ │
│  └───────────┘  └───────────┘  └───────────┘  └──────────┘ │
└──────────────────────────────────────────────────────────────┘
                              ↓
┌──────────────────────────────────────────────────────────────┐
│                   Client Request Processing                   │
│                                                                │
│  1. Receive command from client socket                        │
│  2. Parse command (protocol.c)                                │
│  3. Execute command handler                                   │
│  4. Generate response                                         │
│  5. Send response to client                                   │
│  6. Close socket (if QUIT) or continue receiving             │
└──────────────────────────────────────────────────────────────┘
```

## Data Flow

### Connection Flow
```
Client → TCP SYN → Server
Server → SYN-ACK → Client
Client → ACK → Server
Server: accept() returns client socket
Server: Enqueue client socket to task queue
Worker Thread: Dequeue task
Worker Thread: Enter recv() loop
```

### Request/Response Flow
```
Client: send("PING\n")
    ↓
Server: recv() in worker thread
    ↓
Server: process_command("PING")
    ↓
Server: Generate response "PONG\n"
    ↓
Server: send("PONG\n")
    ↓
Client: recv() and display
```

## Thread Synchronization

### Task Queue Protection

```c
pthread_mutex_lock(&pool->queue_mutex);

// Critical section: modify queue
if (pool->task_queue_tail == NULL) {
    pool->task_queue_head = task;
} else {
    pool->task_queue_tail->next = task;
}
pool->task_queue_tail = task;

// Signal waiting worker
pthread_cond_signal(&pool->queue_cond);

pthread_mutex_unlock(&pool->queue_mutex);
```

### Worker Thread Wait Pattern

```c
pthread_mutex_lock(&pool->queue_mutex);

// Wait for task or shutdown
while (pool->task_queue_head == NULL && !pool->shutdown) {
    pthread_cond_wait(&pool->queue_cond, &pool->queue_mutex);
}

// Get task if available
Task* task = pool->task_queue_head;
if (task != NULL) {
    pool->task_queue_head = task->next;
}

pthread_mutex_unlock(&pool->queue_mutex);

// Execute outside critical section
if (task != NULL) {
    task->function(task->arg);
}
```

### Active Client Counter

```c
// Thread-safe increment
pthread_mutex_lock(&clients_mutex);
active_clients++;
pthread_mutex_unlock(&clients_mutex);

// Thread-safe read
pthread_mutex_lock(&clients_mutex);
int count = active_clients;
pthread_mutex_unlock(&clients_mutex);
```

## Memory Management

### Allocation Points

1. **Thread Pool Creation**
   - Allocates `ThreadPool` structure
   - Allocates array of `pthread_t`
   - All freed in `thread_pool_destroy()`

2. **Task Creation**
   - Allocates `Task` structure for each client
   - Freed after worker completes processing

3. **Client Info**
   - Allocated in `main()` for each accepted connection
   - Freed in `handle_client()` after processing

### Memory Leak Prevention

- All `malloc()` paired with `free()`
- Cleanup in signal handler
- Valgrind-tested for leaks
- RAII-style resource management in functions

## Error Handling Strategy

### System Call Checks

Every system call is checked:

```c
if (socket() < 0) {
    log_error("socket() failed: %s", strerror(errno));
    cleanup();
    return EXIT_FAILURE;
}
```

### Graceful Degradation

- Failed thread creation: destroy pool, exit
- Failed task allocation: close socket, continue
- Client disconnect: log, cleanup, continue
- Invalid command: send error, continue

## Performance Characteristics

### Time Complexity

- Task enqueue: O(1) - append to tail
- Task dequeue: O(1) - remove from head
- Command processing: O(1) for all commands
- Client tracking: O(1) with mutex

### Space Complexity

- Thread pool: O(T) where T = thread count
- Task queue: O(C) where C = concurrent clients
- Per-client memory: ~1KB (stack + buffers)

### Bottlenecks

1. **Single Accept Thread**
   - Mitigated by: Fast accept() + quick handoff to pool
   - Alternative: SO_REUSEPORT multi-accept

2. **Mutex Contention**
   - Task queue mutex held briefly
   - Client counter mutex held briefly
   - Logger mutex for atomic writes

3. **Blocking I/O**
   - recv() blocks per connection
   - Mitigated by: Thread pool parallelism
   - Alternative: Non-blocking + epoll

## Scalability Analysis

### Vertical Scaling (Single Machine)

| Metric | Limit | Note |
|--------|-------|------|
| Concurrent Connections | 1000+ | Limited by file descriptors |
| Thread Pool Size | 8-16 optimal | Depends on CPU cores |
| Request Rate | 10,000+ req/s | For simple commands |
| Memory per Connection | ~1KB | Minimal overhead |

### Configuration for Different Loads

**Low Load (< 50 clients)**
```
THREAD_POOL_SIZE=2
MAX_CONNECTIONS=50
```

**Medium Load (50-500 clients)**
```
THREAD_POOL_SIZE=4
MAX_CONNECTIONS=500
```

**High Load (500+ clients)**
```
THREAD_POOL_SIZE=8
MAX_CONNECTIONS=1000
# Consider ulimit -n 65536
```

## Comparison: Thread Pool vs Thread-Per-Client

| Aspect | Thread Pool | Thread-Per-Client |
|--------|-------------|-------------------|
| Memory | O(T + C) | O(C × stack_size) |
| Creation Overhead | One-time | Per connection |
| Context Switching | Moderate | High (many threads) |
| Scalability | Excellent | Poor (>100 clients) |
| Complexity | Higher | Lower |
| Resource Predictability | High | Low |

## Security Considerations

### Current Implementation

- No authentication
- No encryption
- Basic input validation
- No rate limiting

### Production Enhancements Needed

1. **Authentication**: API keys or tokens
2. **Encryption**: TLS/SSL support
3. **Rate Limiting**: Per-client request limits
4. **Input Validation**: Command length limits, injection protection
5. **Resource Limits**: Connection timeout, max request size

## Testing Strategy

### Unit Tests
- Protocol parsing
- Configuration loading
- Command handlers

### Integration Tests
- Client-server communication
- Concurrent connections
- Graceful shutdown

### Load Tests
- 50+ concurrent clients
- Sustained request rate
- Memory leak detection

### Stress Tests
- Maximum connections
- Rapid connect/disconnect
- Malformed input handling

## Future Enhancements

### Performance
- [ ] Non-blocking I/O with epoll
- [ ] Zero-copy buffer management
- [ ] Connection pooling
- [ ] HTTP/2 protocol support

### Features
- [ ] TLS/SSL encryption
- [ ] Authentication system
- [ ] Binary protocol support
- [ ] Request pipelining
- [ ] WebSocket support

### Operations
- [ ] Prometheus metrics
- [ ] Distributed tracing
- [ ] Hot reload configuration
- [ ] Dynamic thread pool sizing
- [ ] Admin API for statistics

## References

### Books
- Stevens, W. Richard. "Unix Network Programming, Volume 1"
- Kerrisk, Michael. "The Linux Programming Interface"
- Butenhof, David. "Programming with POSIX Threads"

### Online Resources
- man pages: socket(2), pthread_create(3), epoll(7)
- Beej's Guide to Network Programming
- POSIX Threads Programming (LLNL tutorial)
