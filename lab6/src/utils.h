#ifndef UTILS_H
#define UTILS_H

#include <fuse3/fuse.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <time.h>

#define MAX_PATH_COMPONENTS 64
#define TIMESTAMP_BUFFER_SIZE 64
#define PATH_BUFFER_SIZE 4096

char* build_fullpath_safe(const char *base, const char *path, int *error_code);

const char* get_timestamp();

char* canonicalize_path(const char *path);

#endif