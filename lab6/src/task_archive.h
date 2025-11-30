#ifndef TASK_ARCHIVE_H
#define TASK_ARCHIVE_H

#include <fuse.h>

const struct fuse_operations *task_archive_get_operands(const char *archive_path);

#endif // TASK_ARCHIVE_H