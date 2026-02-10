#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>

// Task structure for the queue
typedef struct Task {
    void (*function)(void* arg);
    void* arg;
    struct Task* next;
} Task;

// Thread pool structure
typedef struct {
    pthread_t* threads;
    int thread_count;
    
    Task* task_queue_head;
    Task* task_queue_tail;
    
    pthread_mutex_t queue_mutex;
    pthread_cond_t queue_cond;
    
    int shutdown;
} ThreadPool;

// Create and initialize thread pool
ThreadPool* thread_pool_create(int num_threads);

// Add a task to the queue
int thread_pool_add_task(ThreadPool* pool, void (*function)(void*), void* arg);

// Shutdown and destroy thread pool
void thread_pool_destroy(ThreadPool* pool);

#endif // THREAD_POOL_H
