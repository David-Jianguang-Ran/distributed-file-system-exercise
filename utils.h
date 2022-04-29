//
// Created by dran on 4/23/22.
//

#ifndef NS_PA_4_UTILS_H
#define NS_PA_4_UTILS_H

#include <stdio.h>
#include <time.h>

#include "constants.h"
#include "message.h"
#include "chunk-record.h"

size_t HEADER_BUFFER = sizeof(struct message_header) + sizeof(struct chunk_info);

int send_file_data(struct message_header* header, struct chunk_info* info, int socket_fd, FILE* source);
// this function expects the headers are NOT in the socket, just data bytes
int receive_file_data(int socket_fd, FILE* destination, long int length);

// makes a timestamp with millisecond resolution
long int make_timestamp();

// original may not be divisible by SERVERS,
// the last chunk may not be exactly the same length
int calculate_chunk_length(FILE* original);
// this function is so jank, I'm actually ashamed of it.
int hash_then_get_remainder(char* filename);
// the current position of the stream will not change after this function
int get_file_length(FILE* target);
// will start copying from the current position for both files
// if source runs out before bytes that's ok
int copy_bytes_to_file(FILE* source, FILE* destination, int bytes);

// try to copy item into buffer, if buffer full then send first then copy
int copy_into_buffer_or_send(char* buffer, int* buffer_tail, int buffer_max, int socket_fd, void* to_copy, int copy_length);

// simple wrapper for strncmp, only match up to strlen(command)
// all strings must be zero terminated
// return 0 for no match, 1 for match
int matches_command(char* target, char* command);
int matches_command_case_insensitive(char* target, char* command);

// returns SUCCESS after all up to length is sent
// returns FAIl if connection is already closed
int try_send_in_chunks(int socket_fd, char* buffer, int length);
#endif //NS_PA_4_UTILS_H
