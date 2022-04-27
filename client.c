//
// Created by dran on 4/23/22.
//

#include <sys/socket.h>
#include <netdb.h>

#include "constants.h"
#include "name-table.h"
#include "chunk-record.h"
#include "message.h"
#include "utils.h"

// TODO : rework keep connection, centralize control

struct addrinfo* SERVER_ADDRESS[SERVERS];

int put_file(char* filename, int sockets_to_server[SERVERS], int keep_connection);
int get_file(char* filename, int sockets_to_server[SERVERS], int keep_connection);
int list_all_files(int sockets_to_server[SERVERS]);
// query one server for all filenames stored, add then to name_table
int query_file_names(int server_socket, struct table_element* name_table);
// query all servers and return pointer to the most recent valid set of chunks
struct chunk_set* get_valid_chunk_set(char* filename, int sockets_to_server[SERVERS], int keep_connection);
// query one server for all chunks of a named file, saves to chunk_table
int query_chunk_info(int sockets_to_server[SERVERS], int server_num, char* filename, struct chunk_set* chunk_table);

long int make_timestamp();

int get_file(char* filename, int sockets_to_server[SERVERS], int keep_connection) {
    int result;
    int i;
    struct chunk_set* valid_set;
    char header_buffer[HEADER_BUFFER + 1] = "\0";
    int header_buffer_tail = 0;
    struct message_header* header;
    struct chunk_info* chunk_info;
    char file_name_buffer[MAX_FILENAME_LENGTH + 1] = "\0";
    FILE* local;

    valid_set = get_valid_chunk_set(filename, sockets_to_server, 1);
    if (valid_set == NULL) {
        printf("%s is incomplete\n", filename);
        return SUCCESS;
    }

    sprintf(file_name_buffer, "./%s", filename);
    local = fopen(file_name_buffer, "w");
    if (local == NULL) {
        printf("%s [unable to open local file]\n", filename);
        free(valid_set);
        return FAIL;
    }

    for (i = 0; i < SERVERS; i++) {
        // build and send request to server for chunk[i]
        header = (struct message_header*) header_buffer;
        message_header_set(header, filename, chunk_list, 1);
        chunk_info = (struct chunk_info*) header_buffer + sizeof(struct message_header);
        *chunk_info = valid_set->chunks[i];
        chunk_info_to_network(chunk_info);
        header_buffer_tail = sizeof(struct message_header) + sizeof(struct chunk_info);
        result = try_send_in_chunks(sockets_to_server[valid_set->chunks[i].server_num],
                                    header_buffer, header_buffer_tail);
        if (result == FAIL) {
            printf("%s [failed to send request to server]\n", filename);
            fclose(local);
            free(valid_set);
            return FAIL;
        }

        // read response header
        memset(header_buffer, 0, HEADER_BUFFER);
        result = recv(sockets_to_server[valid_set->chunks[i].server_num], header_buffer, HEADER_BUFFER, 0);
        if (result < sizeof(struct message_header)) {
            return FAIL;
        }
        message_header_from_network(header);
        if (header->type != chunk_data) {
            return FAIL;
        }
        chunk_info_from_network(chunk_info);

        // receive file body, not header
        result = receive_file_data(sockets_to_server[valid_set->chunks[i].server_num], local, chunk_info->length);

        if (result == FAIL) {
            printf("%s [failed to receive chunk data from server]\n", filename);
            free(valid_set);
            return FAIL;
        }
    }

    fclose(local);
    free(valid_set);
    return SUCCESS;
}

int put_file(char* filename, int sockets_to_server[SERVERS], int keep_connection) {
    int has_failed;
    int i;
    int result;
    struct chunk_info chunks_to_send[SERVERS * 2];
    FILE* chunks[SERVERS];

    char header_buffer[HEADER_BUFFER + 1] = '\0';
    struct message_header* header;

    // make chunk - server map

    // make chunks

    // send each partition
    for (i = 0; i < SERVERS * 2; i++) {
        // some servers may be unavailable
        if (sockets_to_server[chunks_to_send[i].server_num] == -1) {
            has_failed += 1;
            continue;
        }

        fseek(chunks[chunks_to_send[i].chunk_num], 0, SEEK_SET);
        result = send_file_data(header, chunks_to_send[i],
                                sockets_to_server[chunks_to_send[i].server_num],
                                chunks[chunks_to_send[i].chunk_num]);
        if (result == FAIL) {
            // fail but other chunks should still be sent
            has_failed += 1;
        }
    }
    if (has_failed) {
        printf("%s put failed\n", filename);
    }
    return SUCCESS;
}

int list_all_files(int sockets_to_server[SERVERS]) {
    struct table_element* name_table = NULL;
    struct table_element* current_name;
    struct chunk_set* valid_set;
    int i;

    // query each server for file names,
    // duplicate names will have no effect
    for (i = 0; i < SERVERS; i++) {
        query_file_names(sockets_to_server[i], name_table);
    }
    // query chunk for each name, names without a valid chunk_set means incomplete
    for (current_name = name_table; current_name != NULL; current_name = current_name->hh.next) {
        valid_set = get_valid_chunk_set(current_name->file_name, sockets_to_server, 1);
        if (valid_set == NULL) {
            printf("%s [incomplete]\n", current_name->file_name);
        } else {
            printf("%s\n", current_name->file_name);
        }
        free(valid_set);
    }
    name_table_free(name_table);
    return SUCCESS;
}

// why not make names structs? then this function could basically be the same as query_chunk_info?
// because most names will not be close to max name length,
// transmitting structs will mean a lot of empty / garbage byte being sent
// I just can't bring myself to do such a wasteful thing, even if it makes my life easier
int query_file_names(int server_socket, struct table_element* name_table) {
    char com_buffer[COM_BUFFER_SIZE + 1] ="\0";
    int com_buffer_tail = 0;
    struct message_header* header;
    int result;
    char* name_start;
    char* name_end;

    // send query to server
    header = (struct message_header*) com_buffer;
    message_header_set(header, filename, name_query, 1);
    com_buffer_tail += sizeof(struct message_header);
    result = try_send_in_chunks(server_socket, com_buffer, com_buffer_tail);
    if (result == FAIL) {
        return FAIL;
    }

    // parse response, FAIL on malformed, wrong typed message header
    result = recv(server_socket, com_buffer, COM_BUFFER_SIZE, 0);
    if (result < sizeof(struct message_header)) {
        return FAIL;  // TODO : print error here
    }
    message_header_from_network(header);
    if (header->type != name_list) {
        return FAIL;
    }


}

struct chunk_set* get_valid_chunk_set(char* filename, int sockets_to_server[SERVERS], int keep_connection) {
    struct chunk_set* chunk_table = NULL;
    struct chunk_set* found_set = NULL;
    struct chunk_set* return_set = NULL;
    int i;
    int result;

    // query each server for file chunks
    for (i = 0; i < SERVERS; i++) {
        if (sockets_to_server[i] != -1) {
            query_chunk_info(sockets_to_server, i, filename, chunk_table);
        }
    }

    found_set = find_latest_valid_set(chunk_table);
    if (found_set != NULL) {  // allocate another heap obj to save the result because chunk_table will be freed soon
        return_set = malloc(sizeof(chunk_set));
        *return_set = *found_set;
    }
    chunk_set_free(chunk_table);
    return return_set;
}

int query_chunk_info(int sockets_to_server[SERVERS], int server_num, char* filename, struct chunk_set* chunk_table) {
    char com_buffer[sizeof(struct message_header) + sizeof(struct chunk_info) + 1] = "\0";
    int com_buffer_tail = 0;
    struct message_header* header;
    struct chunk_info* found_chunk;
    int result;

    // send query to server
    header = (struct message_header*) com_buffer;
    message_header_set(header, filename, chunk_query, 1);
    com_buffer_tail += sizeof(struct message_header);
    result = try_send_in_chunks(sockets_to_server[server_num], com_buffer, com_buffer_tail);
    if (result == FAIL) {
        return FAIL;
    }

    // receive header then receive one chunk_info at a time
    result = recv(sockets_to_server[server_num], com_buffer, sizeof(struct message_header) + sizeof(struct chunk_info), 0);
    if (result < sizeof(struct message_header)) {
        return FAIL;  // TODO : print error here
    }
    message_header_from_network(header);
    if (header->type != chunk_list) {
        return FAIL;
    }
    found_chunk = (struct chunk_info)com_buffer + sizeof(struct message_header);
    chunk_info_from_network(found_chunk);
    while (found_chunk->chunk_num != -1) {
        found_chunk->server_num = server_num;
        chunk_set_add(chunk_table, found_chunk);
        result = recv(sockets_to_server[server_num], com_buffer + sizeof (struct message_header), sizeof(struct chunk_info), 0);
        if (result < sizeof(struct chunk_info)) {
            return FAIL;
        }
        chunk_info_from_network(found_chunk);
    }
    return SUCCESS;
}

