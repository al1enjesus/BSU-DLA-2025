#ifndef OPERATIONS_H
#define OPERATIONS_H


#define _GNU_SOURCE
#include <fuse.h>
#include <sys/stat.h>


int init_fuse_environment(const char *source_dir, const char *mode_str);
void cleanup_fuse_environment(void);

int build_fullpath(const char *path, char *out, size_t outlen, int allow_nonexistent);


void log_operation(const char *op, const char *path, int result);


extern struct fuse_operations passthrough_oper;


#endif // OPERATIONS_H
