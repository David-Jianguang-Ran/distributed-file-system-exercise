//
// Created by dran on 4/23/22.
//

#ifndef NS_PA_4_CHUNK_RECORD_H
#define NS_PA_4_CHUNK_RECORD_H

#include "uthash.h"
#include "constants.h"

struct chunk_info {
    int server_num;
    long int timestamp;
    int chunk_num;
    int length;  // this field may not be populated
};

// a set of 4 valid chunks belonging to the same file and same version
struct chunk_set {
    char file_name[MAX_FILENAME_LENGTH];
    long int timestamp;  // this is hash table key
    struct chunk_info chunks[SERVERS];

    UT_hash_handle hh;
};

struct chunk_table {
    // char file_name[MAX_FILENAME_LENGTH];
    struct chunk_set* head;
};

struct chunk_info chunk_info_create();
// the following functions convert to and from network byte order
void chunk_info_to_network(struct chunk_info* target);
void chunk_info_from_network(struct chunk_info* target);

struct chunk_table* chunk_table_create();
void chunk_table_free(struct chunk_table* table);
// copies the chunk info into the corresponding set based on timestamp and chunk_num
// will overwrite previous chunk_info with the same timestamp and chunk_num
int chunk_table_add(struct chunk_table* table, struct chunk_info* to_add);
struct chunk_set* find_latest_valid_set(struct chunk_table* table);

// private functions below
// sort chunk_set based on timestamp, highest timestamp comes first
int compare_chunk_set_timestamp(struct chunk_set* a, struct chunk_set* b);


#endif //NS_PA_4_CHUNK_RECORD_H
