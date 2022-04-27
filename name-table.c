//
// Created by dran on 4/27/22.
//

#include "name-table.h"

#include <string.h>

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

int name_table_free(struct table_element* table) {
    struct table_element* current;
    struct table_element* temp;

    HASH_ITER(hh, table, current, temp) {
        free(current);
    }
    return SUCCESS;
}