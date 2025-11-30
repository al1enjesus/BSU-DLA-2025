#ifndef TASK_PROXY_H
#define TASK_PROXY_H

#include <fuse.h>

const struct fuse_operations *task_proxy_get_operands(const char *source_path);

#endif // TASK_PROXY_H