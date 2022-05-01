//
// Created by dran on 4/23/22.
//

#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <stdio.h>
#include <unistd.h>

#include "constants.h"
#include "thread-safe-job-stack.h"
#include "thread-safe-file.h"
#include "chunk-record.h"
#include "utils.h"

#define DEBUG 2

char* STORAGE_DIR;
safe_file_t* STD_OUT;
int SHOULD_SHUTDOWN;

void shutdown_signal_handler(int sig_num) {
    SHOULD_SHUTDOWN = 1;
}

void* worker_main(void* job_stack);

// TODO : decide whether to close socket when encountering error
int handle_name_query(int client_socket);
int handle_chunk_query(int client_socket);

// must peak and make sure there is a valid message and chunk header in socket before calling the following functions
int receive_chunk(int client_socket);
int send_chunk(int client_socket);

int main(int argc, char* argv[]) {
    int result;
    int i;

    if (argc != 3) {
        printf("usage: %s <storage-dir> <port>\n", argv[0]);
        return 1;
    }

    // custom shutdown signal handling
    struct sigaction new_action;

    memset(&new_action, 0, sizeof(struct sigaction));
    new_action.sa_handler = shutdown_signal_handler;
    sigaction(SIGTERM, &new_action, NULL);
    sigaction(SIGINT, &new_action, NULL);

    // create main listener socket
    int listener_socket_fd;
    int connected_fd;
    struct sockaddr_in server_sock;
    socklen_t server_sock_len;

    listener_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listener_socket_fd == -1) {
        printf("error: unable to create socket\n");
        return 1;
    }

    memset(&server_sock, 0, sizeof(struct sockaddr_in));
    server_sock.sin_family = AF_INET;
    server_sock.sin_addr.s_addr = htonl(INADDR_ANY);
    server_sock.sin_port = htons(atoi(argv[2]));
    server_sock_len = sizeof(struct sockaddr_in);
    result = bind(listener_socket_fd, (struct sockaddr *) &server_sock, server_sock_len);
    if (result != SUCCESS) {
        printf("error: unable to bind to port %d", atoi(argv[2]));
        return 1;
    }

    // spawn workers
    pthread_t workers[SERVER_WORKERS];
    job_stack_t* job_stack;

    STD_OUT = safe_init(stdout);
    job_stack = job_stack_construct(SERVER_JOB_STACK_SIZE, SERVER_WORKERS);
    STORAGE_DIR = argv[1];

    for (i = 0; i < SERVER_WORKERS; i++) {
        pthread_create(&workers[i], NULL, worker_main, job_stack);
    }

    // listen for incoming connection and push onto job stack
    SHOULD_SHUTDOWN = 0;
    result = listen(listener_socket_fd, SERVER_JOB_STACK_SIZE);
    if (result == -1) {
        SHOULD_SHUTDOWN = 1;
    } else {
        safe_write(STD_OUT, "server listening for connection\n");
    }
    while (!SHOULD_SHUTDOWN) {
        connected_fd = accept(listener_socket_fd, NULL, NULL);
        result = FAIL;
        while (result == FAIL) {  // job stack push can fail, but the client socket must still be serviced
            result = job_stack_push(job_stack, connected_fd);
        }
    }

    // shutdown routine
    safe_write(STD_OUT, "shutdown signal received, waiting for jobs to finish\n");
    job_stack_signal_finish(job_stack);
    for(i = 0; i < SERVER_WORKERS; i++) {
        pthread_join(workers[i], NULL);
    }

    close(listener_socket_fd);
    job_stack_destruct(job_stack);
    safe_close(STD_OUT);
    return 0;
}

void* worker_main(void* job_stack_ptr) {
    job_stack_t* job_stack = (job_stack_t*) job_stack_ptr;
    int job_result = FAIL;
    int result;
    int client_socket;
    char header_buffer[HEADER_BUFFER_SIZE + 1];
    char print_out_buffer[COM_BUFFER_SIZE + 1];
    struct message_header* header = (struct message_header*) header_buffer;
    struct chunk_info* chunk_header = (struct chunk_info*) header_buffer + sizeof(struct message_header);

    while (job_result != FINISHED) {
        job_result = job_stack_pop(job_stack, &client_socket);
        if (job_result == SUCCESS) {
            // receive just the headers
            memset(header_buffer, 0, HEADER_BUFFER_SIZE);
            result = recv(client_socket, header_buffer, HEADER_BUFFER_SIZE, MSG_PEEK);
            if (result < 0) {
                continue;
            } else if (result < (int) sizeof(struct message_header)) {  // incomplete message, push to bottom of stack for later
                job_stack_push_back(job_stack, client_socket);
                continue;
            }
            message_header_from_network(header);
            sprintf(print_out_buffer, "socket: %d received job type:%d name:%s keep-alive:%d\n",
                    client_socket, header->type, header->filename, header->keep_alive);
            safe_write(STD_OUT, print_out_buffer);

            // switch to work functions based on header
            if (header->type == name_query) {
                result = handle_name_query(client_socket);
            } else if (header->type == chunk_query) {
                result = handle_chunk_query(client_socket);
            } else if ((header->type == chunk_list && result < (int) HEADER_BUFFER_SIZE)
                    || (header->type == chunk_data && result < (int) HEADER_BUFFER_SIZE)) {  // incomplete headers, read you later
                job_stack_push_back(job_stack, client_socket);
                continue;
            } else if (header->type == chunk_list) {
                result = send_chunk(client_socket);
            } else if (header->type == chunk_data) {
                result = receive_chunk(client_socket);
            } else {
                // ?? how did we get here?
                sprintf(print_out_buffer, "invalid header received on socket:%d closing connection\n", client_socket);
                safe_write(STD_OUT, print_out_buffer);
                close(client_socket);
            }
            if (DEBUG) {
                sprintf(print_out_buffer, "socket: %d service complete\n", client_socket);
                safe_write(STD_OUT, print_out_buffer);
            }
            // after service
            if (result == FAIL) {
                sprintf(print_out_buffer, "failed request on socket:%d closing connection\n", client_socket);
                safe_write(STD_OUT, print_out_buffer);
            }
            if (header->keep_alive) {
                job_stack_push_back(job_stack, client_socket);
                continue;
            } else {
                close(client_socket);
            }
        }
    }
    return NULL;
}

int handle_name_query(int client_socket) {
    char com_buffer[COM_BUFFER_SIZE + 1] = "\0";
    int com_buffer_tail = 0;
    int result;
    struct message_header* header = (struct message_header*)com_buffer;
    DIR* file_storage;
    struct dirent* found_in_dir;

    // receive message from client
    result = recv(client_socket, com_buffer, COM_BUFFER_SIZE, 0);

    // prepare response
    message_header_init(header, name_list, 1);
    com_buffer_tail += sizeof(struct message_header);

    file_storage = opendir(STORAGE_DIR);
    if (file_storage == NULL) {
        message_header_init(header, error, 0);
        strcpy(header->filename, "failed to open storage dir\n");
        try_send_in_chunks(client_socket, com_buffer, sizeof(struct message_header));
        return FAIL;
    }
    found_in_dir = readdir(file_storage);
    while (found_in_dir != NULL) {
        if (found_in_dir->d_type == DT_DIR && !matches_command(found_in_dir->d_name, ".")) {  // file storage on disk is structured as: each file is a directory containing chunks with timestamp and chunk number in filename
            result = copy_into_buffer_or_send(com_buffer, &com_buffer_tail, COM_BUFFER_SIZE,
                                                  client_socket, found_in_dir->d_name, strlen(found_in_dir->d_name) + 1);  // make sure to copy the \0 into buffer

            if (DEBUG) {
                printf("found file name %s size: %d\n", found_in_dir->d_name, (int)(strlen(found_in_dir->d_name) + 1));
            }
        }
        found_in_dir = readdir(file_storage);
    }
    // sending zero length strings as a signal for data finished
    result = copy_into_buffer_or_send(com_buffer, &com_buffer_tail, COM_BUFFER_SIZE,
                                      client_socket, "\0\0", 2);
    result = try_send_in_chunks(client_socket, com_buffer, com_buffer_tail);

    closedir(file_storage);
    if (result == SUCCESS) {
        return SUCCESS;
    } else {
        return FAIL;
    }
}

int handle_chunk_query(int client_socket) {
    char com_buffer[COM_BUFFER_SIZE] = "\0";
    int com_buffer_tail = 0;
    struct message_header* header;
    struct chunk_info current_chunk;
    char file_name_buffer[MAX_FILENAME_LENGTH + 64] = "\0";
    int result;
    DIR* file_dir;
    struct dirent* found_in_dir;

    result = recv(client_socket, com_buffer, COM_BUFFER_SIZE, 0);
    com_buffer_tail = result;
    header = (struct message_header*) com_buffer;

    // open directory containing file chunks
    sprintf(file_name_buffer, "%s/%s", STORAGE_DIR, header->filename);
    file_dir = opendir(file_name_buffer);
    if (file_dir == NULL) {
        message_header_init(header, error, 0);
        strcpy(header->filename, "failed to open file dir\n");
        try_send_in_chunks(client_socket, com_buffer, sizeof(struct message_header));
        return FAIL;
    }

    // read directory for valid chunks, copy into buffer and send
    found_in_dir = readdir(file_dir);
    while (found_in_dir != NULL) {
        if (found_in_dir->d_type == DT_REG) {  // file storage on disk is structured as: each file is a directory containing chunks with timestamp and chunk number in filename
            printf("    found chunk file: %s\n", found_in_dir->d_name);
            current_chunk = chunk_info_create();
            result = sscanf(found_in_dir->d_name, "%ld-%d.chunk", &(current_chunk.timestamp), &(current_chunk.chunk_num));
            if (result != 2) {
                found_in_dir = readdir(file_dir);
                continue;
            }
            chunk_info_to_network(&current_chunk);
            result = copy_into_buffer_or_send(com_buffer, &com_buffer_tail, COM_BUFFER_SIZE,
                                              client_socket, &current_chunk,sizeof(struct chunk_info));
            if (result == FAIL) {
                return FAIL;
            }
        }
        found_in_dir = readdir(file_dir);
    }
    // send empty chunk info as a signal for finished
    current_chunk = chunk_info_create();
    chunk_info_to_network(&current_chunk);
    result = copy_into_buffer_or_send(com_buffer, &com_buffer_tail, COM_BUFFER_SIZE,
                                      client_socket, &current_chunk,sizeof(struct chunk_info));
    result = try_send_in_chunks(client_socket, com_buffer, com_buffer_tail);
    if (result == FAIL) {
        return FAIL;
    }

    closedir(file_dir);
    return SUCCESS;
}

int send_chunk(int client_socket) {
    char header_buffer[HEADER_BUFFER_SIZE + 1];
    struct message_header* header;
    struct chunk_info* chunk_header;
    char file_name_buffer[MAX_FILENAME_LENGTH + 64] = "\0";
    int result;
    FILE* chunk_file;

    result = recv(client_socket, header_buffer, HEADER_BUFFER_SIZE, 0);
    header = (struct message_header*) header_buffer;
    message_header_from_network(header);
    chunk_header = (struct chunk_info*) header_buffer + sizeof(struct message_header);
    chunk_info_from_network(chunk_header);

    // open chunk file
    sprintf(file_name_buffer, "%s/%s/%ld-%d.chunk",
            STORAGE_DIR, header->filename, chunk_header->timestamp, chunk_header->chunk_num);
    chunk_file = fopen(file_name_buffer, "r");
    if (chunk_file == NULL) {
        message_header_init((struct message_header*)header_buffer, error, 0);
        strncpy(header->filename, "failed to open chunk file\n", FILENAME_MAX);
        try_send_in_chunks(client_socket, header_buffer, sizeof(struct message_header));
        return FAIL;
    }
    header->type = htons(chunk_data);
    result = send_file_data(header, chunk_header, client_socket, chunk_file);
    fclose(chunk_file);
    return SUCCESS;
}

int receive_chunk(int client_socket) {
    char header_buffer[HEADER_BUFFER_SIZE + 1];
    struct message_header* header;
    struct chunk_info* chunk_header;
    char file_name_buffer[MAX_FILENAME_LENGTH + 64] = "\0";
    char new_file_name_buffer[MAX_FILENAME_LENGTH + 64] = "\0";
    int result;
    struct stat file_as_dir;
    FILE* chunk_file;

    result = recv(client_socket, header_buffer, HEADER_BUFFER_SIZE, 0);
    header = (struct message_header*) header_buffer;
    message_header_from_network(header);
    chunk_header = (struct chunk_info*) (header_buffer + sizeof(struct message_header));
    chunk_info_from_network(chunk_header);

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
        message_header_init((struct message_header*)header_buffer, error, 0);
        strncpy(header->filename, "failed to open file\n", FILENAME_MAX);
        try_send_in_chunks(client_socket, header_buffer, sizeof(struct message_header));
        return FAIL;
    }

    // write data to file
    result = receive_file_data(client_socket, chunk_file, chunk_header->length);

    // move file to actual location after
    fclose(chunk_file);
    sprintf(new_file_name_buffer, "%s/%s/%ld-%d.chunk",
            STORAGE_DIR, header->filename, chunk_header->timestamp, chunk_header->chunk_num);
    result = rename(file_name_buffer, new_file_name_buffer);
    if (result == 0) {
        return SUCCESS;
    } else {
        return FAIL;
    }
}
