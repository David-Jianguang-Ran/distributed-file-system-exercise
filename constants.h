//
// Created by dran on 4/23/22.
//

#ifndef NS_PA_4_CONSTANTS_H
#define NS_PA_4_CONSTANTS_H

#define SERVERS 4
#define SERVER_WORKERS 4
#define SERVER_JOB_STACK_SIZE 64

#define MAX_FILENAME_LENGTH 512
#define COM_BUFFER_SIZE 4096
#define HEADER_BUFFER_SIZE (sizeof(struct message_header) + sizeof(struct chunk_info))

#define CLIENT_CONNECT_TIMEOUT 1
#define CLIENT_CONFIG_FILE "~/dfc.conf"

#define SUCCESS 0
#define FAIL 1
#define FINISHED 2
#define ENQUEUE -1

#endif //NS_PA_4_CONSTANTS_H
