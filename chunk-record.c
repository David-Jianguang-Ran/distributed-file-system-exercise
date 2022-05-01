//
// Created by dran on 4/23/22.
//

#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include "chunk-record.h"


struct chunk_info chunk_info_create() {
    struct chunk_info created;
    created.server_num = -1;
    created.timestamp = -1;
    created.chunk_num = -1;
    created.length_str[0] = '\0';
    return created;
}

void chunk_info_to_network(struct chunk_info* target) {
    target->server_num = htons(target->server_num);
    target->timestamp = htonl(target->timestamp);
    target->chunk_num = htons(target->chunk_num);
//    target->length = htons(target->length);
}

void chunk_info_from_network(struct chunk_info* target) {
    target->server_num = ntohs(target->server_num);
    target->timestamp = ntohl(target->timestamp);
    target->chunk_num = ntohs(target->chunk_num);
//    target->length = ntohs(target->length);
}

void chunk_info_set_length(struct chunk_info* target, int length) {
    sprintf(target->length_str, "%d", length);
}

struct chunk_table* chunk_table_create() {
    struct chunk_table* created;
    created = malloc(sizeof(struct chunk_table));
    created->head = NULL;
    return created;
}

void chunk_table_free(struct chunk_table* table) {
    struct chunk_set* current;
    struct chunk_set* temp;

    HASH_ITER(hh, table->head, current, temp) {
        free(current);
    }
    free(table);
    return;
}

int chunk_table_add(struct chunk_table* table, struct chunk_info* to_add) {
    struct chunk_set* found;
    int i;

    HASH_FIND_INT(table->head, &(to_add->timestamp), found);

    if (found == NULL) {
        found = malloc(sizeof(struct chunk_set));
        for (i = 0; i < SERVERS; i++) {
            found->chunks[i] = chunk_info_create();
        }
        found->timestamp = to_add->timestamp;
        HASH_ADD_INT(table->head, timestamp, found);
    }

    found->chunks[to_add->chunk_num] = *to_add;
    return SUCCESS;
}

struct chunk_set* find_latest_valid_set(struct chunk_table* table) {
    int i;
    int incomplete;
    struct chunk_set* found_set;

    HASH_SORT(table->head, compare_chunk_set_timestamp);

    for (found_set = table->head; found_set != NULL; found_set = found_set->hh.next) {
        incomplete = 0;
        for (i = 0; i < SERVERS; i++) {  // loop through to see if all chunks are valid
            if (found_set->chunks[i].chunk_num != i || found_set->chunks[i].server_num == -1) {
                incomplete = 1;
            }
        }
        if (!incomplete) {
            return found_set;
        }
    }
    // no set have all chunks, return null
    return NULL;
}

int compare_chunk_set_timestamp(struct chunk_set* a, struct chunk_set* b) {
    if (a->timestamp > b->timestamp) {
        return -1;
    } else if (a->timestamp < b->timestamp) {
        return 1;
    } else {
        return 0;
    }
}