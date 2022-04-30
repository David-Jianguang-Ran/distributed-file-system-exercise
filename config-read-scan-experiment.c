//
// Created by dran on 4/29/22.
//

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <wordexp.h>

#define FILE_AT "~/dfc.conf"
#define BUFFER_SIZE 1024

int main(int argc, char* argv[]) {
    FILE* config;
    char config_buffer[BUFFER_SIZE];
    wordexp_t expanded;
    char server_name[BUFFER_SIZE];
    char host_name[BUFFER_SIZE];
    char port[BUFFER_SIZE];
    int result;

    wordexp(FILE_AT, &expanded, 0);

    config = fopen(expanded.we_wordv[0], "r");
    if (config == NULL) {
        printf("cannot open config at: %s, error:%s \n", expanded.we_wordv[0], strerror(errno));
        return 1;
    }
    fgets(config_buffer, 1024, config);
    printf("read: %s\n", config_buffer);
    result = sscanf(config_buffer, "server %s %[^:]:%s", server_name, host_name, port);
    printf("scanned %d: <%s><%s>\n",result, host_name, port);
    return 0;
}