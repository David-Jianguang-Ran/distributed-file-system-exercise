//
// Created by dran on 4/28/22.
//

#include <stdio.h>

int main(int argc, char* argv[]) {
    FILE* hashed;
    char command_buffer[1024];
    char result_buffer[33];
    long long int number;

    if (argc != 2) {
        printf("usage: %s <str-to-hash>\n", argv[0]);
    }

    sprintf(command_buffer, "echo '%s'| md5sum\n", argv[1]);
    hashed = popen(command_buffer, "r");
    fread(result_buffer, sizeof(char), 32, hashed);
    number = *((long long int *)result_buffer);  // this is JANK!
    printf("%lld divide by 4 remainder is %d\n", number, (int)number % 4);
    return 0;
}