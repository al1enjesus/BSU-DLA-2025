#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include "operations.h"

/* Склеивает `g_config.root` и переданный путь `path` в безопасный существующий
	путь в `fullpath`. Возвращает 0 при успехе, отрицательное значение при ошибке.
	Предотвращает простейшие атаки через `..` (реализация учебная).
*/
int join_path(char *fullpath, const char *path);

/* Записывает в `buf` текущую временную метку в читаемом виде (YYYY-MM-DD HH:MM:SS). */
void get_timestamp(char *buf, size_t n);

/* Логирует операцию в stderr в формате: [TIMESTAMP] OP: path (result). */
void log_op(const char *op, const char *path, int result);

/* Применяет ROT13 к буферу длины len (модифицирует буфер in-place). */
void rot13_buf(char *buf, size_t len);

/* Преобразует все байты буфера в верхний регистр (in-place). */
void strtoupper_buf(char *buf, size_t len);

#endif // UTILS_H
