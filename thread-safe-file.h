//
// Created by dran on 4/28/22.
//

#ifndef NS_PA_4_THREAD_SAFE_FILE_H
#define NS_PA_4_THREAD_SAFE_FILE_H

#include <stdio.h>
#include <pthread.h>

struct safe_file {
    FILE* file_o;
    pthread_mutex_t mutex;
};
typedef struct safe_file safe_file_t;

// creates a file_handle on the heap
safe_file_t* safe_init(FILE* stream);
// there MUST NOT be any threads waiting when attempting to close
void safe_close(safe_file_t* file_handle_ptr);
void safe_write(safe_file_t* file_handle, char* to_write);

#endif //NS_PA_4_THREAD_SAFE_FILE_H
