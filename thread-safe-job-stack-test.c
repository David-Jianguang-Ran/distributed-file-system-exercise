//
// Created by dran on 3/11/22.
//

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <pthread.h>


#include "thread-safe-job-stack.h"
#include "thread-safe-file.h"

safe_file_t* STD_OUT;

struct heap_obj {
    int var;
    pthread_mutex_t mutex;
};

void* thread_main(void* arg) {
    job_stack_t* job_stack;
    job_t* current_job;
    int ret_status;

    job_stack = (job_stack_t* )arg;

    while (1) {
        ret_status = job_stack_pop(job_stack, &current_job);
        if (ret_status == FINISHED) {
            return NULL;
        } else if (ret_status == SUCCESS && current_job->request_tail == 0) {
            job_destruct(current_job);
            safe_write(STD_OUT, "finishing\n");
        } else if (ret_status == SUCCESS) {
            current_job->request_tail--;
            job_stack_push_back(job_stack, current_job);
            safe_write(STD_OUT, "popping\n");
        }
    }
}

int main(int argc, char* argv[]) {
    int i;
    int thread_count;
    int jobs;
    int stack_size;
    int ret_status;
    job_stack_t* job_stack;
    job_t* new_job;

    srand(time(NULL));

    if (argc != 4) {
        printf("usage: %s <#jobs> <#threads> <stack-size>\n", argv[0]);
        return 1;
    }
    jobs = atoi(argv[1]);
    thread_count = atoi(argv[2]);
    stack_size = atoi(argv[3]);

    job_stack = job_stack_construct(stack_size,thread_count);
    STD_OUT = safe_init(stdout);

    pthread_t threads[thread_count];
    for (i = 0; i < thread_count; i++) {
        pthread_create(&threads[i], NULL, thread_main, job_stack);
    }

    // push jobs onto stack
    for (i = 0; i < jobs; i++) {
        new_job = job_construct(i + 100);
        new_job->request_tail = rand() % 4;
        ret_status = FAIL;
        while (ret_status == FAIL) {
            ret_status = job_stack_push(job_stack, new_job);
            safe_write(STD_OUT, "pushing\n");
        }
    }

    ret_status = job_stack_signal_finish(job_stack);

    for (i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("all finished!\n");
    safe_close(STD_OUT);
    job_stack_destruct(job_stack);
    return 0;

}