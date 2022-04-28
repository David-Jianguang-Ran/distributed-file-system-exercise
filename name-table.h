//
// Created by dran on 4/27/22.
//

#ifndef NS_PA_4_NAME_TABLE_H
#define NS_PA_4_NAME_TABLE_H

#include "uthash.h"
#include "constants.h"

struct table_element {
    char file_name[MAX_FILENAME_LENGTH];
    UT_hash_handle hh;
};

struct name_table {
    struct table_element* head;
};

// adding duplicate names have no effect and is perfectly safe
struct name_table* name_table_create();
int name_table_free(struct name_table* table);
int name_table_add(struct name_table* table, char* name_string);

#endif //NS_PA_4_NAME_TABLE_H
