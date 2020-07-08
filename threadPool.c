//Alon Shoval 309825172 LATE_SUBMISSION

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "threadPool.h"
#include <string.h>


void *start_work(void*);

/*
 * creates a thread pool with given amount of threads
 */
ThreadPool* tpCreate(int numOfThreads) {
    ThreadPool* tp;
    tp = (ThreadPool*)malloc(sizeof(ThreadPool));
    if (tp == NULL) {
        perror("Error in library call 'malloc' for thread pool");
        return NULL;
    }
    // create task queue
    if ((tp->task_queue = osCreateQueue()) == NULL) {
        perror("Error in library call 'malloc' for task queue");
        free(tp);
        return NULL;
    }
    // initialize all thread pool fields. on error, free allocated space and return NULL
    tp->thread_count = numOfThreads;
    tp->thread_total = numOfThreads;
    if ((tp->threads = (pthread_t*)malloc(numOfThreads * sizeof(pthread_t))) == NULL) {
        perror("Error in library call 'malloc' for threads array");
        free(tp);
        free(tp->task_queue);
        return NULL;
    }
    tp->should_end = 0;
    tp->should_stop_now = 0;
    pthread_mutex_init(&(tp->work_mutex), NULL);
    if ((pthread_mutex_init(&(tp->work_mutex), NULL)) != 0) {
        perror("Error in library call 'mutex_init'");
        free(tp);
        free(tp->task_queue);
        free(tp->threads);
        return NULL;
    }
    if ((pthread_cond_init(&(tp->work_exists), NULL)) != 0) {
        perror("Error in library call 'cond_init'");
        free(tp);
        free(tp->task_queue);
        free(tp->threads);
        return NULL;
    }
    if ((pthread_cond_init(&(tp->work_done), NULL)) != 0) {
        perror("Error in library call 'cond_init'");
        free(tp);
        free(tp->task_queue);
        free(tp->threads);
        return NULL;
    }
    int i;
    char* error_message = "unexpected behaviour may occur\n";
    // start running threads
    for (i = 0; i < numOfThreads; i++) {
        if (pthread_create(&tp->threads[i], NULL, start_work, tp) != 0) {
            perror("error in library call 'pthread_create'");
            write(2, error_message, strlen(error_message));
        }
    }
    // return created thread pool.
    return tp;
}

/*
 * this is where threads get and process their tasks
 */
void *start_work(void* pool) {
    if (pool == NULL) {
        perror("bad argument: no thread pool given to 'start_work'");
        return NULL;
    }
    char* error_message = "unexpected behaviour may occur\n";
    ThreadPool *tp = pool;
    task_t *task;
    // each iteration of the loop, the thread will (attempt to) execute one task.
    while (1) {
        // before every access to task queue, mutex is locked
        if ((pthread_mutex_lock(&tp->work_mutex)) != 0) {
            perror("error in library call 'mutex_lock'");
            write(2, error_message, strlen(error_message));
        }
        /*
         * if queue is empty, thread will wait until signaled that there is a new task in queue.
         * note thread will also be signaled if 'tpDestroy' has been called.
         */
        while (osIsQueueEmpty(tp->task_queue) && !tp->should_end) {
            if ((pthread_cond_wait(&tp->work_exists, &tp->work_mutex)) != 0) {
                perror("error in library call 'cond_wait'");
                write(2, error_message, strlen(error_message));
            }
        }
        /*
         * thread will enter this 'if' statement once it should exit.
         * this can happen either when thread is signaled and queue is empty OR
         * thread should shutdown immediately ('tpDestroy' is called with parameter 0).
         */
        if (osIsQueueEmpty(tp->task_queue) || tp->should_stop_now) {
            break;
        }
        task = osDequeue(tp->task_queue);
        // mutex is released before thread starts executing task.
        if ((pthread_mutex_unlock(&tp->work_mutex)) != 0) {
            perror("error in library call 'mutex_unlock'");
            write(2, error_message, strlen(error_message));
        }
        // thread will execute task and once finished, release allocated memory.
        if (task != NULL) {
            task->computeFunc(task->param);
            free(task);
        }
        else {
            error_message = "error: tried to pull a task from empty queue\n";
            write(2, error_message, strlen(error_message));
        }
    }
    /*
     * thread will reach this code once it is signaled to exit
     */
    error_message = "unexpected behaviour may occur\n";
    tp->thread_count--;
    if ((pthread_cond_signal(&tp->work_done)) != 0) {
        perror("error in library call 'cond_signal'");
        write(2, error_message, strlen(error_message));
    }
    if ((pthread_mutex_unlock(&tp->work_mutex)) != 0) {
        perror("error in library call 'mutex_unlock'");
        write(2, error_message, strlen(error_message));
    }
    return NULL;
}


void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks) {

    OSNode *node;
    OSNode *next_node;
    task_t *task;
    char* error_message = "unexpected behaviour may occur\n";
    int i;
    if (threadPool == NULL) {
        return;
    }
    // lock the mutex before accessing thread pools fields
    if ((pthread_mutex_lock(&threadPool->work_mutex)) != 0) {
        perror("error in library call 'mutex_lock'");
        write(2, error_message, strlen(error_message));
    }
    // check no one called 'tpDestroy' before
    if (threadPool->should_end) {
        if ((pthread_mutex_unlock(&threadPool->work_mutex)) != 0) {
            perror("error in library call 'mutex_unlock'");
            write(2, error_message, strlen(error_message));
        }
        error_message = "error: 'tpDestroy' was called twice on same thread pool\n";
        write(2, error_message, strlen(error_message));
        return;
    }
    // set appropriate boolean field.
    threadPool->should_end = 1;
    if (shouldWaitForTasks == 0) {
        threadPool->should_stop_now = 1;
    }
    // broadcast to all threads that they should finish
    if ((pthread_cond_broadcast(&threadPool->work_exists)) != 0) {
        perror("error in library call 'cond_broadcast'");
        write(2, error_message, strlen(error_message));
    }
    /*
     * each thread that finishes will signal this thread, but thread pool should only be destroyed
     * once all threads have finished, so in this loop, this thread will be signaled by each finishing thread.
     */
    while (threadPool->thread_count != 0) {
        if ((pthread_cond_wait(&threadPool->work_done, &threadPool->work_mutex)) != 0) {
            perror("error in library call 'cond_wait'");
            write(2, error_message, strlen(error_message));
        }
    }
    // make sure all worker threads have exited and released memory.
    for (i = 0; i < threadPool->thread_total; i++) {
        if ((pthread_join(threadPool->threads[i], NULL)) != 0) {
            perror("error in library call 'cond_wait'");
            write(2, error_message, strlen(error_message));
        }
    }
    node = threadPool->task_queue->head;
    // release all memory of tasks left in task queue (if there are any).
    while (node != NULL) {
        next_node = node->next;
        task = node->data;
        free(task);
        free(node);
        node = next_node;
    }
    // destroy the mutex and conditionals
    if ((pthread_mutex_unlock(&threadPool->work_mutex)) != 0) {
        perror("error in library call 'mutex_unlock'");
        write(2, error_message, strlen(error_message));
    }
    if ((pthread_mutex_destroy(&threadPool->work_mutex)) != 0) {
        perror("error in library call 'mutex_destroy'");
        write(2, error_message, strlen(error_message));
    }
    if ((pthread_cond_destroy(&threadPool->work_exists)) != 0) {
        perror("error in library call 'cond_destroy'");
        write(2, error_message, strlen(error_message));
    }
    if ((pthread_cond_destroy(&threadPool->work_done)) != 0) {
        perror("error in library call 'cond_destroy'");
        write(2, error_message, strlen(error_message));
    }
    // free remaining allocated memory
    free(threadPool->task_queue);
    free(threadPool->threads);
    free(threadPool);
}


/*
 * insert a new task to task queue.
 */
int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param) {
    task_t *task;
    char *error_message;
    if (threadPool == NULL) {
        error_message = "error: bad argument given to 'tpInsertTask', thread pool pointer is NULL\n";
        write(2, error_message, strlen(error_message));
        return -1;
    }
    // make sure 'tpDestroy' was not called earlier.
    if (threadPool->should_end) {
        error_message = "error: 'tpDestroy' was called before 'tpInsertTask'\n";
        write(2, error_message, strlen(error_message));
        return -1;
    }
    if (computeFunc == NULL) {
        error_message = "error: bad argument given to 'tpInsertTask', function pointer given is NULL\n";
        write(2, error_message, strlen(error_message));
        return -1;
    }
    if ((task = (task_t*)malloc(sizeof(task_t))) == NULL) {
        perror("Error in library call 'malloc' for new task in 'tpInsertTask'");
        return -1;
    }
    task->computeFunc = computeFunc;
    task->param = param;
    error_message = "unexpected behaviour may occur\n";
    // lock the mutex before accessing task queue
    if ((pthread_mutex_lock(&threadPool->work_mutex)) != 0) {
        perror("error in library call 'mutex_lock'");
        write(2, error_message, strlen(error_message));
    }
    // insert new task to queue.
    osEnqueue(threadPool->task_queue, task);
    if ((pthread_mutex_unlock(&threadPool->work_mutex)) != 0) {
        perror("error in library call 'mutex_unlock'");
        write(2, error_message, strlen(error_message));
    }
    // signal to a waiting thread (if exists) that a new task is ready to be executed
    if ((pthread_cond_signal(&threadPool->work_exists)) != 0) {
        perror("error in library call 'cond_signal'");
        write(2, error_message, strlen(error_message));
    }
    return 0;
}