//
// Created by dran on 4/28/22.
//

#include <stdio.h>

#include "name-table.h"

int main(int argc, char* argv[]) {
    struct name_table* table;
    struct table_element* current;
    int i;

    if (argc < 2) {
        printf("usage: %s <strings to add> ...\n", argv[0]);
        return 1;
    }
    table = name_table_create();
    for (i = 1; i < argc; i++) {
        name_table_add(table, argv[i]);
    }
    printf("iterating table head is null %d\n", table->head == NULL);
    for (current = table->head; current != NULL; current = current->hh.next) {
        printf("%s\n", current->file_name);
    }
    return 0;
}