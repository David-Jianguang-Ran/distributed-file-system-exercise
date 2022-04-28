//
// Created by dran on 4/23/22.
//

#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#include "utils.h"
#include "chunk-record.h"
#include "message.h"

int send_from_file(char* buffer, int* buffer_tail, int buffer_max, int socket_fd, FILE* source, long int length) {
    int result;
    struct chunk_info* chunk_header;

    // find file length and update the header
    chunk_header = (struct chunk_info*) buffer + sizeof(struct message_header);
    fseek(source, 0, SEEK_END);
    chunk_header->length = htonl(ftell(source));
    fseek(source, 0, SEEK_SET);

    // read from disk and send
    result = fread(buffer + *buffer_tail, sizeof(char), buffer_max - *buffer_tail, source);
    buffer_tail += result;
    result = try_send_in_chunks(client_socket, buffer, *buffer_tail);
    if (result == FAIL) {
        return FAIL;
    }

    memset(buffer, 0, buffer_max);
    *buffer_tail = 0;

    result = fread(buffer, sizeof(char), buffer_max, source);
    while (result != 0) {
        *buffer_tail = result;
        result = try_send_in_chunks(client_socket, buffer, buffer_tail);
        if (result == FAIL) {
            return FAIL;
        }
        result = fread(buffer, sizeof(char), buffer_max, source);
    }

    return SUCCESS;
}

int receive_to_file(char* buffer, int* buffer_tail, int buffer_max, int socket_fd, FILE* destination ,long int length) {
    int result;
    long int length_done = 0;

    // TODO : not checking result after writing to disk might be trouble
    result = fwrite(buffer + sizeof(struct message_header) + sizeof(struct chunk_info), sizeof(char),
            *buffer_tail - sizeof(struct message_header) - sizeof(struct chunk_info), destination);
    length_done += (long int)result;

    memset(buffer, 0, buffer_max);
    *buffer_tail = 0;

    while (length_done < length) {
        result = recv(socket_fd, buffer, buffer_max, 0);
        *buffer_tail = result;
        result = fwrite(buffer, sizeof(char ), *buffer_tail, destination);
        length_done += result;
    }
    return SUCCESS;
}

int hash_then_get_remainder(char* filename) {
    FILE* hashed;
    char command_buffer[FILENAME_MAX * 2];
    char result_buffer[33];
    long long int result_numeric;

    sprintf(command_buffer, "echo '%s' | md5sum\n", filename);
    hashed = popen(command_buffer, "r");
    fread(result_buffer, sizeof(char), 32, hashed);
    result_numeric = *((long long int*)result_buffer);
    return (int) result_numeric % SERVERS;
}

int copy_into_buffer_or_send(char* buffer, int* buffer_tail, int buffer_max,
                             int socket_fd, void* to_copy, int copy_length) {
    int result;
    if (*buffer_tail + copy_length > buffer_max) {  // buffer full, time to send it, clear buffer, and resume
        result = try_send_in_chunks(socket_fd, buffer, *buffer_tail);
        if (result == FAIL) {
            return FAIL;
        }
        memset(buffer, 0, buffer_max);
        *buffer_tail = 0;
    }
    memcpy(buffer + *buffer_tail, to_copy, copy_length);
    *buffer_tail += copy_length;
    return SUCCESS;
}

// TODO : add bounds checking and failing
void copy_into_buffer(char* target, int* target_tail, char* content) {
    strcpy(target + *target_tail, content);  // TODO: address possible buffer overflow here
    *target_tail += strlen(content);
}

int try_send_in_chunks(int socket_fd, char* buffer, int length) {
    int ret_status;
    int bytes_sent;
    char garbage_buffer[1000];

    if (socket_fd == -1) {  // why would anyone call try_send with no receiver? code reuse
        return SUCCESS;
    }

    bytes_sent = 0;
    while (bytes_sent < length) {

        // first see if client is still there
        ret_status = recv(socket_fd, garbage_buffer, 999, MSG_PEEK | MSG_DONTWAIT);
        if (ret_status == 0) {
            return FAIL;
        }
        // do send based on what's already sent;
        // printf("sending %d bytes\n", length - bytes_sent);
        ret_status = send(socket_fd, buffer + bytes_sent, length - bytes_sent, MSG_NOSIGNAL);
        if (ret_status == -1) {
            // not sure what will cause this,
            // it's my understanding that if client is not there the whole process just get killed
            return FAIL;
        }
        bytes_sent += ret_status;
    }
    return SUCCESS;
}