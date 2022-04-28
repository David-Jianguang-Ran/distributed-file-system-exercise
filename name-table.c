//
// Created by dran on 4/27/22.
//

#include "name-table.h"

#include <string.h>

struct name_table* name_table_create() {
    struct name_table* created;
    created = malloc(sizeof(struct name_table));
    created->head = NULL;
    return created;
}

int name_table_add(struct name_table* table, char* name_string) {
    struct table_element* found_element;

    HASH_FIND_STR(table->head, name_string, found_element);

    if (found_element == NULL) {
        found_element = malloc(sizeof(struct table_element));
        strncpy(found_element->file_name, name_string, MAX_FILENAME_LENGTH);
        HASH_ADD_STR(table->head, file_name, found_element);
    }
    return SUCCESS;
}

int name_table_free(struct name_table* table) {
    struct table_element* current;
    struct table_element* temp;

    HASH_ITER(hh, table->head, current, temp) {
        free(current);
    }
    free(table);
    return SUCCESS;
}