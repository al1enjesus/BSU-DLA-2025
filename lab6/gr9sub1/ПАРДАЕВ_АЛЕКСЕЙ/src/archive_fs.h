#ifndef ARCHIVE_FS_H
#define ARCHIVE_FS_H

#include <fuse3/fuse.h>

struct fuse_operations *get_archive_ops(const char *tar_path);

#endif
