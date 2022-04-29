//
// Created by dran on 4/28/22.
//

#include "thread-safe-file.h"
#include <stdlib.h>

safe_file_t* safe_init(FILE* stream) {
    safe_file_t* file_handle;
    file_handle = malloc(sizeof(safe_file_t));
    file_handle->file_o = stream;
    pthread_mutex_init(&(file_handle->mutex), NULL);  // no need for pshared since all threads are in one process.
    return file_handle;
}

void safe_close(safe_file_t* file_handle) {
    pthread_mutex_destroy(&(file_handle->mutex));
    fclose(file_handle->file_o);
    free(file_handle);
}

void safe_write(safe_file_t* file_handle, char* to_write) {
    pthread_mutex_lock(&(file_handle->mutex));
    fprintf(file_handle->file_o, "%s", to_write);
    pthread_mutex_unlock(&(file_handle->mutex));
}