//
// Created by dran on 4/23/22.
//

#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <wordexp.h>
#include <errno.h>

#include "constants.h"
#include "name-table.h"
#include "chunk-record.h"
#include "utils.h"

#define DEBUG 0

// TODO : rework keep connection, centralize control
struct addrinfo* server_address[SERVERS];

// this function will populate an array of pointers to addrinfo,one to each server
int get_server_address();
// will try to connect to servers and populate sockets to server
// will set socket to -1 if connection fails, will not affect existing open sockets
int connect_to_servers(int* sockets_to_server);

int put_file(char* filename, int sockets_to_server[SERVERS], int keep_connection);
int get_file(char* filename, int sockets_to_server[SERVERS], int keep_connection);
int list_all_files(int sockets_to_server[SERVERS]);

// query one server for all filenames stored, add then to name_table
int query_file_names(int server_socket, struct name_table* name_table);
// query all servers and return pointer to the most recent valid set of chunks
struct chunk_set* get_valid_chunk_set(char* filename, int sockets_to_server[SERVERS], int keep_connection);
// query one server for all chunks of a named file, saves to chunk_table
int query_chunk_info(int sockets_to_server[SERVERS], int server_num, char* filename, struct chunk_table* chunk_table);

int main(int argc, char* argv[]) {
    int i;
    int result;
    int has_failed = 0;

    // check arguments
    if (argc < 2) {
        printf("usage: %s <command> [filename] ...\n", argv[0]);
        return 1;
    }

    // get connections to server
    int sockets_to_server[SERVERS];

    result = get_server_address();
    if (result == FAIL) {
        printf("failed to get server address info\n");
        return 1;
    }
    for (i = 0; i < SERVERS; i++) {
        sockets_to_server[i] = -1;
    }
    result = connect_to_servers(sockets_to_server);

    // load filenames into name table
    struct name_table* names;
    struct table_element* current;

    names = name_table_create();
    for (i = 2; i < argc; i++) {
        name_table_add(names, argv[i]);
    }

    // do work
    if (matches_command_case_insensitive(argv[1], "list")) {
        result = list_all_files(sockets_to_server);
    } else if (matches_command_case_insensitive(argv[1], "get")) {
        for (current = names->head; current != NULL; current = current->hh.next) {
            result = get_file(current->file_name, sockets_to_server, current->hh.next != NULL);
            if (result == FAIL) {
                has_failed += 1;
            }
        }
    } else if (matches_command_case_insensitive(argv[1], "put")) {
        for (current = names->head; current != NULL; current = current->hh.next) {
            result = put_file(current->file_name, sockets_to_server, current->hh.next != NULL);
            if (result == FAIL) {
                has_failed += 1;
            }
        }
    } else {
        printf("usage: %s <command> [filename] ...\n", argv[0]);
        has_failed = 1;
    }

    name_table_free(names);
    for (i = 0; i < SERVERS; i++) {
        freeaddrinfo(server_address[i]);
        if (sockets_to_server[i] != -1) {
            close(sockets_to_server[i]);
        }
    }
    if (!has_failed) {
        return 0;
    } else {
        return 1;
    }
}

int get_server_address() {
    FILE* config_file;
    wordexp_t expanded_path;
    char config_buffer[MAX_FILENAME_LENGTH * 2];
    char server_name[MAX_FILENAME_LENGTH];
    char host_name[MAX_FILENAME_LENGTH];
    char port[16];
    struct addrinfo server_address_hints;
    int result;
    char* read_str;
    int i;

    wordexp(CLIENT_CONFIG_FILE, &expanded_path, 0);
    config_file = fopen(expanded_path.we_wordv[0], "r");
    if (config_file == NULL) {
        return FAIL;
    } else if (DEBUG) {
        printf("config file loaded\n");
    }

    memset(&server_address_hints, 0, sizeof(struct addrinfo));
    server_address_hints.ai_family = AF_INET;
    server_address_hints.ai_socktype = SOCK_STREAM;
    server_address_hints.ai_protocol = IPPROTO_TCP;
    for (i = 0; i < SERVERS; i++) {
        read_str = fgets(config_buffer, MAX_FILENAME_LENGTH * 2, config_file);
        if (read_str == NULL) {
            server_address[i] = NULL;
            continue;
        }
        result = sscanf(config_buffer, "server %s %[^:]:%s\n", server_name, host_name, port);
        if (result < 3) {
            server_address[i] = NULL;
            continue;
        } else if (DEBUG) {
            printf("found config for server: %d at %s:%s\n", i, host_name, port);
        }
        result = getaddrinfo(host_name, port, &server_address_hints, server_address + i);
        if (result != 0) {
            server_address[i] = NULL;
            printf("    address not found: %s\n", gai_strerror(result));
        }
    }
    fclose(config_file);
    return SUCCESS;
}

int connect_to_servers(int* sockets_to_server) {
    struct timeval timeout;
    int result;
    int i;

    if (DEBUG) {
        printf("trying to connect to servers:\n");
    }
    // connect to each server
    timeout.tv_sec = CLIENT_CONNECT_TIMEOUT;
    timeout.tv_usec = 0;
    for (i = 0; i < SERVERS; i++) {
        if (sockets_to_server[i] != -1) {
            if (DEBUG) {
                printf("    server: %d socket:%d <already exist>\n", i, sockets_to_server[i]);
            }
            continue;  // connection already exist
        } else if (server_address[i] == NULL) {
            if (DEBUG) {
                printf("    server: %d socket:%d <no addrinfo>\n", i, sockets_to_server[i]);
            }
            continue;
        }
        sockets_to_server[i] = socket(AF_INET, SOCK_STREAM, 0);
        // TODO : decide if receive time out of 1 sec is good for normal client operations
        result = setsockopt(sockets_to_server[i], SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval));
        result = connect(sockets_to_server[i], server_address[i]->ai_addr, server_address[i]->ai_addrlen);
        if (result != 0) {
            close(sockets_to_server[i]);
            sockets_to_server[i] = -1;
        }
        if (DEBUG) {
            printf("    server: %d socket:%d <new>\n", i, sockets_to_server[i]);
        }
    }


    return SUCCESS;
}

int get_file(char* filename, int sockets_to_server[SERVERS], int keep_connection) {
    int result;
    int i;
    struct chunk_set* valid_set;
    char header_buffer[HEADER_BUFFER_SIZE + 1] = "\0";
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
        chunk_info = (struct chunk_info*) (header_buffer + sizeof(struct message_header));
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
        memset(header_buffer, 0, HEADER_BUFFER_SIZE);
        result = recv(sockets_to_server[valid_set->chunks[i].server_num], header_buffer, HEADER_BUFFER_SIZE, 0);
        if (result < (int)sizeof(struct message_header)) {
            return FAIL;
        }
        message_header_from_network(header);
        if (header->type != chunk_data) {
            return FAIL;
        }
        chunk_info_from_network(chunk_info);

        // receive file body, not header
        result = receive_file_data(sockets_to_server[valid_set->chunks[i].server_num], local, atoi(chunk_info->length_str));

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
    int has_failed = 0;
    int i = 0;
    int result = 0;
    int map_offset = 0;
    int chunk_offset = 0;
    long int timestamp = 0;
    char file_name_buffer[FILENAME_MAX + 64];
    struct chunk_info chunks_to_send[SERVERS * 2];
    FILE* original;
    FILE* chunk_files[SERVERS];
    struct message_header header;

    // try to open file
    original = fopen(filename, "r");
    if (original == NULL) {
        printf("cannot open file <%s> %s\n", filename, strerror(errno));
        return FAIL;
    } else if (get_file_length(original) < 4) {
        printf("put failed file <%s> is too small (less than 4 bytes)\n", filename);
        return FAIL;
    }

    timestamp = make_timestamp();
    map_offset = hash_then_get_remainder(filename);
    chunk_offset = calculate_chunk_length(original) + 1;
    message_header_set(&header, filename, chunk_data, 1);

    // partition file into chunk_files
    for (i = 0; i < SERVERS; i++) {
        sprintf(file_name_buffer, "./ctmp/%ld-%d", timestamp, i);
        chunk_files[i] = fopen(file_name_buffer,"w+");
        copy_bytes_to_file(original, chunk_files[i], chunk_offset);
    }

    // make chunk - server map
    for (i = 0; i < SERVERS; i++) {
        chunks_to_send[i].server_num = i;
        chunks_to_send[i].chunk_num = (i - map_offset + SERVERS) % SERVERS;
        chunks_to_send[i].timestamp = timestamp;

        chunks_to_send[i + SERVERS].server_num = i;
        chunks_to_send[i + SERVERS].chunk_num = (i - map_offset + 1 + SERVERS) % SERVERS;
        chunks_to_send[i + SERVERS].timestamp = timestamp;
    }

    // send each partition
    for (i = 0; i < SERVERS * 2; i++) {
        // some servers may be unavailable
        if (sockets_to_server[chunks_to_send[i].server_num] == -1) {
            // fail but other chunks should still be sent
            has_failed += 1;
            continue;
        }
        if (DEBUG) {
            // TODO : server number should start at 1 here
            printf("sending chunk %d to server %d\n", chunks_to_send[i].chunk_num, chunks_to_send[i].server_num);
        }
        result = send_file_data(&header, &chunks_to_send[i],
                                sockets_to_server[chunks_to_send[i].server_num],
                                chunk_files[chunks_to_send[i].chunk_num]);
        if (result == FAIL) {
            has_failed += 1;
        }
        if (DEBUG && result == FAIL) {
            printf("    failed\n");
        }
    }
    if (has_failed) {
        printf("%s put failed\n", filename);
    }

    // clean up temporary file
    for (i = 0; i < SERVERS; i++) {
        fclose(chunk_files[i]);
        sprintf(file_name_buffer, "./ctmp/%ld-%d", timestamp, i);
        remove(file_name_buffer);
    }

    return SUCCESS;
}

int list_all_files(int sockets_to_server[SERVERS]) {
    struct name_table* name_table;
    struct table_element* current_name;
    struct chunk_set* valid_set;
    int i;

    name_table = name_table_create();
    // query each server for file names,
    // duplicate names will have no effect
    for (i = 0; i < SERVERS; i++) {
        query_file_names(sockets_to_server[i], name_table);
    }
    // query chunk for each name, names without a valid chunk_set means incomplete
    for (current_name = name_table->head; current_name != NULL; current_name = current_name->hh.next) {
        connect_to_servers(sockets_to_server);
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
int query_file_names(int server_socket, struct name_table* name_table) {
    char com_buffer[COM_BUFFER_SIZE + 1];
    int com_buffer_tail = 0;
    struct message_header* header;
    int result;
    char* name_start;
    char* name_end;

    memset(com_buffer, '\0', COM_BUFFER_SIZE + 1);
    // send query to server
    header = (struct message_header*) com_buffer;
    message_header_set(header, "\0", name_query, 1);
    com_buffer_tail += sizeof(struct message_header);
    result = try_send_in_chunks(server_socket, com_buffer, com_buffer_tail);
    if (result == FAIL) {
        return FAIL;
    }

    // parse response, FAIL on malformed, wrong typed message header
    result = recv(server_socket, com_buffer, COM_BUFFER_SIZE, 0);
    if (result < (int)sizeof(struct message_header)) {
        return FAIL;  // TODO : print error here
    }
    message_header_from_network(header);
    if (header->type != name_list) {
        return FAIL;
    }

    // read name strings and add to name table
    name_end = com_buffer + sizeof(struct message_header) - 1;
    while (1) {
        name_start = name_end + 1;
        name_end = strchr(name_start, '\0');
        if (name_end == com_buffer + COM_BUFFER_SIZE + 1) { // incomplete name, move to start of buffer and continue reading
            com_buffer_tail = name_end - name_start - 1;
            memmove(com_buffer, name_start, name_end - name_start - 1);
            result = recv(server_socket, com_buffer + com_buffer_tail, COM_BUFFER_SIZE - com_buffer_tail, 0);
            if (result == 0) {
                return SUCCESS;  // server closed socket, but we might have some useful data so let's just limp along
            }
            com_buffer_tail += result;
            name_start = com_buffer;
            name_end = strchr(name_start, '\0');
            if (name_end == com_buffer + COM_BUFFER_SIZE + 1) {
                return FAIL;  // names should never be longer than buffer something has gone terribly wrong
            } else {
                name_table_add(name_table,name_start);
            }
        } else if (name_end == name_start) {  // zero length string, the message is finished
            return SUCCESS;
        } else {
            name_table_add(name_table, name_start);
        }
    }
}

struct chunk_set* get_valid_chunk_set(char* filename, int sockets_to_server[SERVERS], int keep_connection) {
    struct chunk_table* table;
    struct chunk_set* found_set = NULL;
    struct chunk_set* return_set = NULL;
    int i;
    int result;

    table = chunk_table_create();
    // query each server for file chunks
    for (i = 0; i < SERVERS; i++) {
        if (sockets_to_server[i] != -1) {
            query_chunk_info(sockets_to_server, i, filename, table);
        }
    }

    found_set = find_latest_valid_set(table);
    if (found_set != NULL) {  // allocate another heap obj to save the result because chunk_table will be freed soon
        return_set = malloc(sizeof(struct chunk_set));
        *return_set = *found_set;
    }
    chunk_table_free(table);
    return return_set;
}

int query_chunk_info(int sockets_to_server[SERVERS], int server_num, char* filename, struct chunk_table* chunk_table) {
    char header_buffer[HEADER_BUFFER_SIZE+ 1];
    int header_buffer_tail = 0;
    struct message_header* header;
    struct chunk_info* found_chunk;
    int result;

    // send query to server
    memset(header_buffer, 0, HEADER_BUFFER_SIZE + 1);
    header = (struct message_header*) header_buffer;
    message_header_set(header, filename, chunk_query, 1);
    header_buffer_tail += sizeof(struct message_header);
    result = try_send_in_chunks(sockets_to_server[server_num], header_buffer, header_buffer_tail);
    if (result == FAIL) {
        return FAIL;
    }

    // receive header then receive one chunk_info at a time
    result = recv(sockets_to_server[server_num], header_buffer, HEADER_BUFFER_SIZE, 0);
    if (result < (int) HEADER_BUFFER_SIZE) {
        return FAIL;  // TODO : print error here
    }
    message_header_from_network(header);
    if (header->type != chunk_list) {
        return FAIL;
    }
    found_chunk = (struct chunk_info*) (header_buffer + sizeof(struct message_header));
    chunk_info_from_network(found_chunk);

    while (found_chunk->chunk_num != -1 && found_chunk->chunk_num != 65535) {
        found_chunk->server_num = server_num;
        if (DEBUG) {
            printf("found timestamp:%ld chunk:%d server:%d\n", found_chunk->timestamp, found_chunk->chunk_num, found_chunk->server_num);
        }
        chunk_table_add(chunk_table, found_chunk);

        result = recv(sockets_to_server[server_num], header_buffer + sizeof (struct message_header), sizeof(struct chunk_info), 0);
        if (result < (int) sizeof(struct chunk_info)) {
            return FAIL;
        }
        chunk_info_from_network(found_chunk);
    }
    return SUCCESS;
}

