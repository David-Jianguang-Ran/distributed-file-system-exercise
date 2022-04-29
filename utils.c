//
// Created by dran on 4/23/22.
//

#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>

#include "utils.h"
#include "chunk-record.h"

void message_header_init(struct message_header* target, enum message_type type, int keep_alive) {
    target->filename[0] = '\0';
    target->type = htons(type);
    target->keep_alive = htons(keep_alive);
}

void message_header_set(struct message_header* target, char* filename, enum message_type type, int keep_alive) {
    if (filename != NULL) {
        strncpy(target->filename, filename, MAX_FILENAME_LENGTH);
    } else {
        target->filename[0] = '\0';
    }
    target->type = htons(type);
    target->keep_alive = htons(keep_alive);
}

void message_header_from_network(struct message_header* target) {
    target->type = ntohs(target->type);
    target->keep_alive = ntohs(target->keep_alive);
}

int send_file_data(struct message_header* header_arg, struct chunk_info* info_arg, int socket_fd, FILE* source) {
    char file_buffer[COM_BUFFER_SIZE + 1];
    int file_buffer_tail = 0;
    char com_buffer[COM_BUFFER_SIZE + 1];
    int com_buffer_tail = 0;
    struct message_header* header;
    struct chunk_info* info;
    int result;

    // populate header
    header = (struct message_header*) com_buffer;
    info = (struct chunk_info*) com_buffer + sizeof(struct message_header);
    com_buffer_tail = sizeof(struct message_header) + sizeof(struct chunk_info);
    *header = *header_arg;
    header->keep_alive = htons(header->keep_alive);
    *info = *info_arg;
    chunk_info_to_network(info);

    // read some data from file, send when com buffer is filled
    // do until source file is exhausted
    file_buffer_tail = fread(file_buffer, sizeof(char), COM_BUFFER_SIZE, source);
    while (file_buffer_tail != 0) {
        result = copy_into_buffer_or_send(com_buffer, &com_buffer_tail, COM_BUFFER_SIZE,
                                          socket_fd,file_buffer, file_buffer_tail);
        if (result == FAIL) {
            return FAIL;
        }
        file_buffer_tail = fread(file_buffer, sizeof(char), COM_BUFFER_SIZE, source);
    }
    result = try_send_in_chunks(socket_fd, com_buffer, com_buffer_tail);
    if (result == FAIL) {
        return FAIL;
    } else {
        return SUCCESS;
    }
}

int receive_file_data(int socket_fd, FILE* destination, long int length) {
    char com_buffer[COM_BUFFER_SIZE + 1];
    int com_buffer_tail = 0;
    int result;
    int bytes_done = 0;

    while (bytes_done < length) {
        result = recv(socket_fd, com_buffer, COM_BUFFER_SIZE, 0);
        if (result == 0) {
            return FAIL;
        } else {
            com_buffer_tail = result;
        }
        bytes_done += fwrite(com_buffer, sizeof(char ), com_buffer_tail, destination);
    }
    return SUCCESS;
}

long int make_timestamp() {
    struct timeval now;
    gettimeofday(&now, NULL);
    return  (long int) now.tv_sec * 1000 + (long int) now.tv_usec / 1000;
}

int calculate_chunk_length(FILE* original) {
    return get_file_length(original) / SERVERS;
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

int get_file_length(FILE* target) {
    int location;
    int end;

    location = ftell(target);
    fseek(target, 0, SEEK_END);
    end = ftell(target);
    fseek(target, location, SEEK_SET);
    return end;
}

// this function assumes writing to disk should generally be successful
int copy_bytes_to_file(FILE* source, FILE* destination, int bytes) {
    char buffer[COM_BUFFER_SIZE + 1];
    int buffer_tail = 0;
    int to_read = 0;
    int copied = 0;

    to_read = COM_BUFFER_SIZE < (bytes - copied) ? COM_BUFFER_SIZE : (bytes - copied);
    while (to_read > 0) {
        buffer_tail = fread(buffer, sizeof(char), COM_BUFFER_SIZE, source);
        if (buffer_tail == 0) {  // running out of source early is allowed
            return SUCCESS;
        }
        copied += fwrite(buffer, sizeof(char ), buffer_tail, destination);
        to_read = COM_BUFFER_SIZE < (bytes - copied) ? COM_BUFFER_SIZE : (bytes - copied);
    }
    return SUCCESS;

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

int matches_command(char* target, char* command) {
    if (strncmp(target, command, strlen(command)) == 0) {
        return 1;
    } else {
        return 0;
    }
}

int matches_command_case_insensitive(char* target, char* command) {
    if (strncasecmp(target, command, strlen(command)) == 0) {
        return 1;
    } else {
        return 0;
    }
}

int try_send_in_chunks(int socket_fd, char* buffer, int length) {
    int ret_status;
    int bytes_sent;
    char garbage_buffer[9];

    if (socket_fd == -1) {  // why would anyone call try_send with no receiver? code reuse
        return FAIL;
    }

    bytes_sent = 0;
    while (bytes_sent < length) {

        // first see if client is still there
        ret_status = recv(socket_fd, garbage_buffer, 8, MSG_PEEK | MSG_DONTWAIT);
        if (ret_status == 0) {
            return FAIL;
        }
        // do send based on what's already sent;
        // printf("sending %d bytes\n", length - bytes_sent);
        ret_status = send(socket_fd, buffer + bytes_sent, length - bytes_sent, MSG_NOSIGNAL);
        if (ret_status == -1) {
            return FAIL;
        }
        bytes_sent += ret_status;
    }
    return SUCCESS;
}