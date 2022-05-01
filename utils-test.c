//
// Created by dran on 4/30/22.
//

#include <stdio.h>

#include "utils.h"



int test_timestamp() {
    long int timestamp;

    timestamp = make_timestamp();
    printf("result: %ld\n", timestamp);
    return 0;
}

int test_file_copy(char* from_file, char* to_file) {
    FILE* from;
    FILE* to;

    from = fopen(from_file, "r");
    if (from == NULL) {
        return 1;
    }
    to = fopen(to_file, "w");

    copy_bytes_to_file(from, to, get_file_length(from));
    printf("please diff %s and %s\n", from_file, to_file);
    return 0;
}

int main(int argc, char* argv[]) {
    test_file_copy("./wine3.jpg", "./wine3c.jpg");
    test_timestamp();

    return 0;
}