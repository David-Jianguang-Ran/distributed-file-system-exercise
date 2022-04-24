//
// Created by dran on 3/5/22.
//

#ifndef NS_PA_4_THREAD_SAFE_JOB_STACK_H
#define NS_PA_4_THREAD_SAFE_JOB_STACK_H

#include <pthread.h>

struct job_stack {
    void** content_base;
    int top;
    int max_job_count;
    int reserve_job_count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    int finished;
};
typedef struct job_stack job_stack_t;

// the # of reserve_slots must be greater than # of consumers that also want to produce sometimes
job_stack_t* job_stack_construct(int max_jobs, int reserve_slots);
// job stack MUST BE empty before calling destruct
void job_stack_destruct(job_stack_t* to_free);

// this function is used for push new jobs onto stack, should be used by job producers
// will block until stack has less than `max_job_count` items
int job_stack_push(job_stack_t* stack, void* ptr_in);
// TODO : upgrade this object to a priority queue based on job->expiration_time
// this function should be used by job consumers that sometimes have to produce too
// there is no wait for push, there will always be space in the reserve_slots
// this function is for pushing existing jobs back onto stack
// pushes to the bottom of stack
int job_stack_push_back(job_stack_t* stack, void* ptr_in);
// this function returns FINISHED `job_stack_signal_finish` has been called
int job_stack_pop(job_stack_t* stack, void** ptr_out);
int job_stack_signal_finish(job_stack_t* stack);

// private functions below
void* job_stack_get_item(job_stack_t* stack, int index);
void job_stack_set_item(job_stack_t* stack, int index, void* ptr_in);

#endif //NS_PA_4_THREAD_SAFE_JOB_STACK_H
