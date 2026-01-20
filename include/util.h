/*
 * util.h - Utility functions
 */
#ifndef TEDIT_UTIL_H
#define TEDIT_UTIL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* String utilities */
char *str_trim(char *s);
char *str_dup(const char *s);
int str_ends_with(const char *s, const char *suffix);
int str_starts_with(const char *s, const char *prefix);

/* Path utilities */
const char *path_basename(const char *path);
const char *path_extension(const char *path);
void path_dirname(const char *path, char *dir, size_t max);
void path_join(char *out, size_t max, const char *a, const char *b);

/* File utilities */
char *file_read_all(const char *path, size_t *len);
int file_write_all(const char *path, const char *data, size_t len);
int file_exists(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* TEDIT_UTIL_H */

