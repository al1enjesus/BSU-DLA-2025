#ifndef TASK_MONITOR_H
#define TASK_MONITOR_H

#include <fuse.h>

const struct fuse_operations *task_monitor_get_operands(const char *source_path);

#endif // TASK_MONITOR_H