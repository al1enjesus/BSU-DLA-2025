#include <ctype.h>
#include <string.h>
#include "operations.h"

void rot13(char *buf, size_t size) {
    for (size_t i = 0; i < size; i++) {
        char c = buf[i];

        if ('A' <= c && c <= 'Z')
            buf[i] = ((c - 'A' + 13) % 26) + 'A';
        else if ('a' <= c && c <= 'z')
            buf[i] = ((c - 'a' + 13) % 26) + 'a';
    }
}

void to_uppercase(char *buf, size_t size) {
    for (size_t i = 0; i < size; i++)
        buf[i] = toupper(buf[i]);
}
