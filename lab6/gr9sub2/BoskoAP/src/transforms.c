#include "transforms.h"
#include <stddef.h>

void rot13_inplace(char *buf, size_t n) {
    if (!buf || n == 0) return;
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)buf[i];
        if (c >= 'a' && c <= 'z') {
            buf[i] = (char)('a' + (c - 'a' + 13) % 26);
        } else if (c >= 'A' && c <= 'Z') {
            buf[i] = (char)('A' + (c - 'A' + 13) % 26);
        }
    }
}

void uppercase_inplace(char *buf, size_t n) {
    if (!buf || n == 0) return;
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)buf[i];
        if (c >= 'a' && c <= 'z') {
            buf[i] = (char)(c - 'a' + 'A');
        }
    }
}
