//
// Created by dran on 4/23/22.
//

#ifndef NS_PA_4_MESSAGE_H
#define NS_PA_4_MESSAGE_H

#include <arpa/inet.h>
#include "constants.h"

// name query asks for all file names on server
// name list is a stream of file names as zero terminated strings
// chunk query asks for all chunk info for a given file name
// chunk list is a stream of chunk info objects for a given file name
// when the client send a single item chunk_list it is a request for data of the named chunk
// chunk data is a single chunk info object followed by a certain bytes of data stated in chunk info
enum message_type {name_query, name_list, chunk_query, chunk_list, chunk_data, error};

struct message_header {
    char filename[MAX_FILENAME_LENGTH];
    enum message_type type;
};

void message_header_init(struct message_header* target, enum message_type type) {
    target->filename[0] = '\0';
    target->type = htons(type);
}

#endif //NS_PA_4_MESSAGE_H
