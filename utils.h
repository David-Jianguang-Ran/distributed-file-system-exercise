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

// TODO: rewrite send and recevive to work with data length header
int send_from_file(char* buffer, int* buffer_tail, int buffer_max, int socket_fd, FILE* source);
// this function expects a header and chunk header in buffer
int receive_to_file(char* buffer, int* buffer_tail, int buffer_max, int socket_fd, FILE* source);

int send_file_data(struct message_header* header, struct chunk_info* info, int socket_fd, FILE* source);
int receive_file_data(int socket_fd, FILE* source, long int length);

// try to copy item into buffer, if buffer full then send first then copy
int copy_into_buffer_or_send(char* buffer, int* buffer_tail, int buffer_max, int socket_fd, void* to_copy, int copy_length);

// simple wrapper for strncmp, only match up to strlen(command)
// all strings must be zero terminated
// return 0 for no match, 1 for match
int matches_command(char* target, char* command);
int matches_command_case_insensitive(char* target, char* command);

// this function DOES NOT check str boundary
void copy_into_buffer(char* target, int* target_tail, int target_max, char* content);
// returns SUCCESS after all up to length is sent
// returns FAIl if connection is already closed
int try_send_in_chunks(int socket_fd, char* buffer, int length);
#endif //NS_PA_4_UTILS_H
