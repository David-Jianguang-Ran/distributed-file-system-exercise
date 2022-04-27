//
// Created by dran on 4/23/22.
//

#include <arpa/inet.h>

#include "chunk_record.h"


struct chunk_info chunk_info_create() {
    struct chunk_info created;
    created.server_num = -1;
    created.timestamp = -1;
    created.chunk_num = -1;
    created.length = -1;
    return created;
}

void chunk_info_to_network(struct chunk_info* target) {
    target->server_num = htons(target->server_num);
    target->timestamp = htonl(target->timestamp);
    target->chunk_num = htons(target->chunk_num);
    target->length = htonl(target->length);
}

void chunk_info_from_network(struct chunk_info* target) {
    target->server_num = ntohs(target->server_num);
    target->timestamp = ntohl(target->timestamp);
    target->chunk_num = ntohs(target->chunk_num);
    target->length = ntohl(target->length);
}

int chunk_set_free(struct chunk_set* ptr_to_set) {
    struct chunk_set* current;
    struct chunk_set* temp;

    HASH_ITER(hh, ptr_to_set, current, temp) {
        free(current);
    }
    return SUCCESS;
}

int chunk_set_add(struct chunk_set* ptr_to_set, struct chunk_info* to_add) {
    struct chunk_set* found;
    int i;

    HASH_FIND_INT(ptr_to_set, &(to_add->timestamp), found);

    if (found == NULL) {  // this is the only place where new chunk_set is created
        found = malloc(sizeof(struct chunk_set));
        found->file_name[0] = '\0';
        found->timestamp = to_add->timetamp;
        for (i = 0; i < SERVERS; i++) {
            found[i] = chunk_info_create();
        }
        HASH_ADD_INT(ptr_to_set, timestamp, found);
    }

    found->chunks[to_add->chunk_num] = *to_add;

    return SUCCESS;
}

struct chunk_set* find_latest_valid_set(struct chunk_set* ptr_to_set) {
    int i;
    int incomplete;
    struct chunk_set* found_set;

    HASH_SORT(ptr_to_set, compare_chunk_set_timestamp);

    for (found_set = ptr_to_set; found_set != NULL; found_set = found_set->hh.next) {
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