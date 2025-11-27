#ifndef MON_FS_H
#define MON_FS_H

#include <fuse3/fuse.h>

struct fuse_operations *get_monitor_ops(const char *root);

#endif
