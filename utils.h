//
// Created by dran on 4/23/22.
//

#ifndef NS_PA_4_UTILS_H
#define NS_PA_4_UTILS_H

#include <stdio.h>
#include <time.h>

#include "constants.h"
#include "chunk-record.h"


// name query asks for all file names on server
// name list is a stream of file names as zero terminated strings
// chunk query asks for all chunk info for a given file name
// chunk list is a stream of chunk info objects for a given file name
// when the client send a single item chunk_list it is a request for data of the named chunk
// chunk data is a single chunk info object followed by a certain bytes of data stated in chunk info
enum message_type {name_query, name_list, chunk_query, chunk_list, chunk_data, error};

struct message_header {
    char filename[MAX_FILENAME_LENGTH + 1];
    enum message_type type;
    int keep_alive;
};

void message_header_init(struct message_header* target, enum message_type type, int keep_alive);
void message_header_set(struct message_header* target, char* filename, enum message_type type, int keep_alive);
void message_header_from_network(struct message_header* target);



int send_file_data(struct message_header* header, struct chunk_info* info, int socket_fd, FILE* source);
// this function expects the headers are NOT in the socket, just data bytes
int receive_file_data(int socket_fd, FILE* destination, int length);

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
