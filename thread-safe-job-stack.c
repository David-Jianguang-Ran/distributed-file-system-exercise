//
// Created by dran on 3/5/22.
//

#include <stdlib.h>
#include <stdio.h>

#include "thread-safe-job-stack.h"

#define DEBUG 0


job_stack_t* job_stack_construct(int max_jobs, int reserve_slots) {
    job_stack_t* created;
    created = malloc(sizeof(job_stack_t));
    if (created == NULL) {
        return created;
    }
    created->content_base = malloc((max_jobs + reserve_slots + 1) * sizeof(int));
    pthread_cond_init(&(created->not_empty), NULL);
    pthread_cond_init(&(created->not_full), NULL);
    pthread_mutex_init(&(created->mutex), NULL);
    created->finished = 0;
    created->max_job_count = max_jobs;
    created->reserve_job_count = reserve_slots;
    created->top = 0;
    return created;
}

void job_stack_destruct(job_stack_t* to_free) {
    free(to_free->content_base);
    pthread_mutex_destroy(&(to_free->mutex));
    pthread_cond_destroy(&(to_free->not_full));
    pthread_cond_destroy(&(to_free->not_empty));
    free(to_free);
}

int job_stack_push(job_stack_t* stack, int data_in) {
    pthread_mutex_lock(&(stack->mutex));
    if (DEBUG) printf("push locked, top: %d, fin: %d\n", stack->top, stack->finished);
    if (stack->finished) {
        return FINISHED;
    }
    // if stack has more than max items,
    // wait until a consumer has popped some off stack and try again
    if (stack->top >= stack->max_job_count) {
        pthread_cond_wait(&(stack->not_full), &(stack->mutex));
        pthread_mutex_unlock(&(stack->mutex));
        return FAIL;
    }
    job_stack_set_item(stack, stack->top, data_in);
    stack->top++;
    if (DEBUG) printf("    pushed: %d\n", job_stack_get_item(stack, stack->top - 1));
    pthread_cond_signal(&(stack->not_empty));
    pthread_mutex_unlock(&(stack->mutex));
    return SUCCESS;
}

int job_stack_push_back(job_stack_t* stack, int data_in) {
    int i;
    if (stack->finished) {
        if (DEBUG) printf("    push back %d, FAIL\n", job_stack_get_item(stack, 0));
        return FAIL;
    }
    pthread_mutex_lock(&(stack->mutex));
    // push_back shouldn't need to wait for space
    // push ptr_in to the bottom of stack, first move all existing items up one
    for (i = stack->top; i > 0; i--) {
        job_stack_set_item(stack, i, job_stack_get_item(stack, i - 1));
    }
    stack->top++;
    job_stack_set_item(stack, 0, data_in);
    if (DEBUG) printf("    push back %d, top: %d\n", job_stack_get_item(stack, 0), stack->top);
    pthread_cond_signal(&(stack->not_empty));
    pthread_mutex_unlock(&(stack->mutex));
    return SUCCESS;
}

int job_stack_pop(job_stack_t* stack, int* data_out) {
    pthread_mutex_lock(&(stack->mutex));
    if (DEBUG) printf("pop locked, top: %d, fin: %d\n", stack->top, stack->finished);
    // unless finished, wait until some job has been pushed onto stack and try to pop again
    if (stack->top == 0 && stack->finished == 0) {
        pthread_cond_wait(&(stack->not_empty), &(stack->mutex));
        pthread_mutex_unlock(&(stack->mutex));
        return FAIL;
    } else if (stack->top == 0) {
        pthread_mutex_unlock(&(stack->mutex));
        return FINISHED;
    }
    *data_out = job_stack_get_item(stack, stack->top - 1);
    stack->top--;
    if (DEBUG) printf("    popped: %d\n", job_stack_get_item(stack, stack->top));
    pthread_cond_signal(&(stack->not_full));
    pthread_mutex_unlock(&(stack->mutex));
    return SUCCESS;
}

int job_stack_signal_finish(job_stack_t* stack) {
    pthread_mutex_lock(&(stack->mutex));
    if (DEBUG) printf("finish signal issued\n");
    stack->finished = 1;
    pthread_cond_broadcast(&(stack->not_empty));
    pthread_mutex_unlock(&(stack->mutex));
    return SUCCESS;
}

int job_stack_get_item(job_stack_t* stack, int index) {
    return *(stack->content_base + index);
}

void job_stack_set_item(job_stack_t* stack, int index, int item) {
    *(stack->content_base + index) = item;
}