#ifndef PASSTHROUGH_H
#define PASSTHROUGH_H

#include <fuse3/fuse.h>

struct fuse_operations *get_passthrough_ops(const char *root);

#endif
