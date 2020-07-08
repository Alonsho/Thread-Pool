//Alon Shoval 309825172 LATE-SUBMISSION

#ifndef __THREAD_POOL__
#define __THREAD_POOL__

#include "osqueue.h"
#include <pthread.h>


/*
 * struct for a task that is to be executed by a thread
 */
typedef struct
{
    void (*computeFunc) (void *);
    void *param;
}task_t;


/*
 * thread pool
 */
typedef struct thread_pool
{

    // queue of tasks
    OSQueue *task_queue;
    // mutex for sync
    pthread_mutex_t work_mutex;
    /*
     * condition to notify threads that new work has been added to task queue.
     * NOTE: this same condition is also used to notify thread that 'tpDestroy() has been called.
     */
    pthread_cond_t work_exists;
    // condition to notify main thread that a thread is done and exiting.
    pthread_cond_t work_done;
    // total amount of threads
    size_t thread_total;
    // current amount of threads that are not done yet
    size_t thread_count;
    // boolean for immediate shutdown of thread pool (not to empty queue)
    int should_stop_now;
    // boolean for regular shutdown (finish all tasks from queue first)
    int should_end;
    // all threads of thread pool
    pthread_t* threads;
}ThreadPool;
/*
 * function details can be found in source file
 */


ThreadPool* tpCreate(int numOfThreads);

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks);

int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param);

#endif
