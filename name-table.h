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

// adding duplicate names have no effect and is perfectly safe
int name_table_add(struct table_element* table, char* name_string);
int name_table_free(struct table_element* table);

#endif //NS_PA_4_NAME_TABLE_H
