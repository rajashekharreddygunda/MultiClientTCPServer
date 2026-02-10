#include "thread_pool.h"
#include "logger.h"
#include <stdlib.h>
#include <stdio.h>

static void* worker_thread(void* arg) {
    ThreadPool* pool = (ThreadPool*)arg;
    
    while (1) {
        pthread_mutex_lock(&pool->queue_mutex);
        
        // Wait for tasks or shutdown signal
        while (pool->task_queue_head == NULL && !pool->shutdown) {
            pthread_cond_wait(&pool->queue_cond, &pool->queue_mutex);
        }
        
        // Check for shutdown
        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->queue_mutex);
            break;
        }
        
        // Get task from queue
        Task* task = pool->task_queue_head;
        if (task != NULL) {
            pool->task_queue_head = task->next;
            if (pool->task_queue_head == NULL) {
                pool->task_queue_tail = NULL;
            }
        }
        
        pthread_mutex_unlock(&pool->queue_mutex);
        
        // Execute task
        if (task != NULL) {
            task->function(task->arg);
            free(task);
        }
    }
    
    log_message(LOG_DEBUG, "Worker thread exiting");
    return NULL;
}

ThreadPool* thread_pool_create(int num_threads) {
    if (num_threads <= 0) {
        return NULL;
    }
    
    ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
    if (pool == NULL) {
        return NULL;
    }
    
    pool->thread_count = num_threads;
    pool->task_queue_head = NULL;
    pool->task_queue_tail = NULL;
    pool->shutdown = 0;
    
    // Initialize mutex and condition variable
    if (pthread_mutex_init(&pool->queue_mutex, NULL) != 0) {
        free(pool);
        return NULL;
    }
    
    if (pthread_cond_init(&pool->queue_cond, NULL) != 0) {
        pthread_mutex_destroy(&pool->queue_mutex);
        free(pool);
        return NULL;
    }
    
    // Allocate thread array
    pool->threads = (pthread_t*)malloc(sizeof(pthread_t) * num_threads);
    if (pool->threads == NULL) {
        pthread_mutex_destroy(&pool->queue_mutex);
        pthread_cond_destroy(&pool->queue_cond);
        free(pool);
        return NULL;
    }
    
    // Create worker threads
    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&pool->threads[i], NULL, worker_thread, pool) != 0) {
            log_message(LOG_ERROR, "Failed to create worker thread %d", i);
            thread_pool_destroy(pool);
            return NULL;
        }
        log_message(LOG_DEBUG, "Created worker thread %d", i);
    }
    
    log_message(LOG_INFO, "Thread pool created with %d threads", num_threads);
    return pool;
}

int thread_pool_add_task(ThreadPool* pool, void (*function)(void*), void* arg) {
    if (pool == NULL || function == NULL) {
        return -1;
    }
    
    Task* task = (Task*)malloc(sizeof(Task));
    if (task == NULL) {
        return -1;
    }
    
    task->function = function;
    task->arg = arg;
    task->next = NULL;
    
    pthread_mutex_lock(&pool->queue_mutex);
    
    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->queue_mutex);
        free(task);
        return -1;
    }
    
    // Add task to queue
    if (pool->task_queue_tail == NULL) {
        pool->task_queue_head = task;
        pool->task_queue_tail = task;
    } else {
        pool->task_queue_tail->next = task;
        pool->task_queue_tail = task;
    }
    
    // Signal a worker thread
    pthread_cond_signal(&pool->queue_cond);
    
    pthread_mutex_unlock(&pool->queue_mutex);
    
    return 0;
}

void thread_pool_destroy(ThreadPool* pool) {
    if (pool == NULL) {
        return;
    }
    
    // Signal shutdown
    pthread_mutex_lock(&pool->queue_mutex);
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->queue_cond);
    pthread_mutex_unlock(&pool->queue_mutex);
    
    // Wait for all threads to finish
    for (int i = 0; i < pool->thread_count; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    
    // Clean up remaining tasks
    pthread_mutex_lock(&pool->queue_mutex);
    Task* task = pool->task_queue_head;
    while (task != NULL) {
        Task* next = task->next;
        free(task);
        task = next;
    }
    pthread_mutex_unlock(&pool->queue_mutex);
    
    // Destroy synchronization primitives
    pthread_mutex_destroy(&pool->queue_mutex);
    pthread_cond_destroy(&pool->queue_cond);
    
    // Free resources
    free(pool->threads);
    free(pool);
    
    log_message(LOG_INFO, "Thread pool destroyed");
}
