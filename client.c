//
// Created by dran on 4/23/22.
//

#include "uthash.h"
#include "constants.h"

struct table_element {
    char file_name[MAX_FILENAME_LENGTH];
    UT_hash_handle hh;
};

// adding duplicate names have no effect and is perfectly safe
int name_table_add(struct table_element* table, char* name_string);

int find_valid_chunk_set(char* filename);
int put_file(char* filename);

long int make_timestamp();


int name_table_add(struct table_element* table, char* name_string) {
    struct table_element* found_element;

    HASH_FIND_STR(table, file_name, found_element);

    if (found_element == NULL) {
        found_element = malloc(sizeof(struct table_element));
        strncpy(found_element->file_name, name_string, MAX_FILENAME_LENGTH);
        HASH_ADD_STR(table, file_name, found_element);
    }
    return SUCCESS;
}