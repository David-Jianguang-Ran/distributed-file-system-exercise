//
// Created by dran on 4/23/22.
//

#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <unistd.h>

#include "constants.h"
#include "thread-safe-job-stack.h"
#include "chunk-record.h"
#include "message.h"
#include "utils.h"

char* STORAGE_DIR;

int worker_main(job_stack_t* job_stack);

int handle_name_query(int client_socket);
int handle_chunk_query(int client_socket, int server_num);
int receive_chunk_data(int client_socket);
int send_chunk_data(int client_socket);

int main(int argc, char* argv[]) {
    return 0;
}

int handle_name_query(int client_socket) {
    char com_buffer[COM_BUFFER_SIZE] = "\0";
    int com_buffer_tail = 0;
    int result;
    DIR file_storage;
    struct dirent* found_in_dir;

    // no need to recv from client because worker_main have peeked the message already
    message_header_init((struct message_header*)com_buffer, name_list);
    com_buffer_tail += sizeof(struct message_header);

    file_storage = opendir(STORAGE_DIR);
    if (file_storage == NULL) {
        message_header_init((struct message_header*)com_buffer, error);
        copy_into_buffer(com_buffer, &com_buffer_tail, COM_BUFFER_SIZE, "failed to open dir\n");
        try_send_in_chunks(client_socket, com_buffer, com_buffer_tail);
        return FAIL;
    }
    found_in_dir = readdir(file_storage);
    while (found_in_dir != NULL) {
        if (found_in_dir->d_type == DT_DIR) {  // file storage on disk is structured as: each file is a directory containing chunks with timestamp and chunk number in filename
            result = copy_into_buffer(com_buffer, &com_buffer_tail, COM_BUFFER_SIZE, found_in_dir->d_name);
            if (result == FAIL) { // buffer full, time to send it, clear buffer, and resume
                result = try_send_in_chunks(client_socket, com_buffer, com_buffer_tail);
                if (result != SUCCESS) {
                    return FAIL;
                }
                com_buffer[0] = '\0';
                com_buffer_tail = 0;
                continue;
            }
        }
        found_in_dir = readdir(file_storage);
    }

    closedir(file_storage);
    close(client_socket);
    return SUCCESS;
}

int handle_chunk_query(int client_socket, int server_num) {
    char com_buffer[COM_BUFFER_SIZE] = "\0";
    int com_buffer_tail = 0;
    struct message_header* header;
    struct chunk_info* current_chunk;
    char file_name_buffer[MAX_FILENAME_LENGTH] = "\0";
    int result;
    DIR file_dir;
    struct dirent* found_in_dir;

    result = recv(client_socket, com_buffer, COM_BUFFER_SIZE, 0);
    com_buffer_tail = result;
    header = (struct message_header*) com_buffer;

    // open directory containing file chunks
    sprintf(file_name_buffer, "%s/%s", STORAGE_DIR, header->filename);
    file_dir = opendir(file_name_buffer);
    if (file_dir == NULL) {
        message_header_init((struct message_header*)com_buffer, error);
        copy_into_buffer(com_buffer, &com_buffer_tail, COM_BUFFER_SIZE, "fail to open file dir\n");
        try_send_in_chunks(client_socket, com_buffer, com_buffer_tail);
        return FAIL;
    }

    // read directory for valid chunks, copy into buffer and send
    found_in_dir = readdir(file_dir);
    while (found_in_dir != NULL) {
        if (found_in_dir->d_type == DT_REG) {  // file storage on disk is structured as: each file is a directory containing chunks with timestamp and chunk number in filename
            if (com_buffer_tail + sizeof(struct chunk_info) > COM_BUFFER_SIZE) { // buffer full, time to send it, clear buffer, and resume
                result = try_send_in_chunks(client_socket, com_buffer, com_buffer_tail);
                if (result == FAIL) {
                    return FAIL;
                }
                com_buffer[0] = '\0';
                com_buffer_tail = 0;
                continue;
            } else {
                current_chunk = (struct chunk_info*) com_buffer + com_buffer_tail;
                result = sscanf(found_in_dir->d_name, "%ld-%d.chunk", &(current_chunk->timestamp), &(current_chunk->chunk_num));
                if (result != 0) {
                    current_chunk->server_num = server_num;
                    chunk_info_to_network(current_chunk);
                    com_buffer_tail += sizeof(struct chunk_info);
                }
            }
        }
        found_in_dir = readdir(file_dir);
    }

    closedir(file_dir);
    close(client_socket);
    return SUCCESS;
}

int send_chunk_data(int client_socket) {
    char com_buffer[COM_BUFFER_SIZE] = "\0";
    int com_buffer_tail = 0;
    struct message_header* header;
    struct chunk_info* chunk_header;
    char file_name_buffer[MAX_FILENAME_LENGTH] = "\0";
    int result;
    FILE* chunk_file;

    result = recv(client_socket, com_buffer, COM_BUFFER_SIZE, 0);
    if (result < (sizeof(struct message_header) + sizeof(struct chunk_info))) {
        message_header_init((struct message_header*)com_buffer, error);
        copy_into_buffer(com_buffer, &com_buffer_tail, COM_BUFFER_SIZE, "malformed headers received\n");
        try_send_in_chunks(client_socket, com_buffer, com_buffer_tail);
        return FAIL; // TODO : refactor error message sending into a util function
    } else {
        com_buffer_tail = result;
        header = (struct message_header*) com_buffer;
        chunk_header = (struct chunk_info*) com_buffer + sizeof(struct message_header);
    }

    // open chunk file
    sprintf(file_name_buffer, "%s/%s/%ld-%d.chunk",
            STORAGE_DIR, header->filename, chunk_header->timestamp, chunk_header->chunk_num);
    chunk_file = fopen(file_name_buffer, "r");
    if (chunk_file == NULL) {
        message_header_init((struct message_header*)com_buffer, error);
        copy_into_buffer(com_buffer, &com_buffer_tail, COM_BUFFER_SIZE, "failed to open file\n");
        try_send_in_chunks(client_socket, com_buffer, com_buffer_tail);
        return FAIL;
    }

    result = send_from_file(com_buffer, &com_buffer_tail, COM_BUFFER_SIZE, client_socket, chunk_file);

    close(client_socket)
    fclose(chunk_file);
    return SUCCESS;
}

int receive_chunk_data(int client_socket) {
    char com_buffer[COM_BUFFER_SIZE] = "\0";
    int com_buffer_tail = 0;
    struct message_header* header;
    struct chunk_info* chunk_header;
    char file_name_buffer[MAX_FILENAME_LENGTH] = "\0";
    int result;
    struct stat file_as_dir;
    FILE* chunk_file;

    result = recv(client_socket, com_buffer, COM_BUFFER_SIZE, 0);
    if (result < (sizeof(struct message_header) + sizeof(struct chunk_info))) {
        message_header_init((struct message_header*)com_buffer, error);
        copy_into_buffer(com_buffer, &com_buffer_tail, COM_BUFFER_SIZE, "malformed headers received\n");
        try_send_in_chunks(client_socket, com_buffer, com_buffer_tail);
        return FAIL; // TODO : refactor error message sending into a util function
    } else {
        com_buffer_tail = result;
        header = (struct message_header*) com_buffer;
        chunk_header = (struct chunk_info*) com_buffer + sizeof(struct message_header);
    }

    // make sure the file_named directory exists
    sprintf(file_name_buffer, "%s/%s", STORAGE_DIR, header->filename);
    result = stat(file_name_buffer, &file_as_dir);
    if (result == -1) {
        mkdir(file_name_buffer, S_IRWXU);
    }

    // open and write data to a temporary file then move to location later to avoid the need to lock file
    sprintf(file_name_buffer, "%s/%s/temp-%ld-%d.chunk",
            STORAGE_DIR, header->filename, chunk_header->timestamp, chunk_header->chunk_num);
    chunk_file = fopen(file_name_buffer, "w");
    if (chunk_file == NULL) {
        message_header_init((struct message_header*)com_buffer, error);
        copy_into_buffer(com_buffer, &com_buffer_tail, COM_BUFFER_SIZE, "failed to open file\n");
        try_send_in_chunks(client_socket, com_buffer, com_buffer_tail);
        return FAIL;
    }

    // write data to disk
    result = receive_to_file(com_buffer, &com_buffer_tail, client_socket, chunk_file);

    // move file to actual location after
    fclose(chunk_file);
    sprintf(com_buffer, "%s/%s/%ld-%d.chunk",
            STORAGE_DIR, header->filename, chunk_header->timestamp, chunk_header->chunk_num);
    result = rename(file_name_buffer, com_buffer);

    close(client_socket);
    if (result == 0) {
        return SUCCESS;
    } else {
        return FAIL;
    }
}
