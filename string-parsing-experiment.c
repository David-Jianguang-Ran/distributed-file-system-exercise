//
// Created by dran on 4/24/22.
//

#include <stdio.h>
#include <string.h>

int main(int argc, char* argv[]) {
    int number;
    long int big_number;

    sscanf("12345-54321", "%ld-%d", &big_number, &number);
    printf("scanned %ld, %d", big_number, number);
    return 0;
}