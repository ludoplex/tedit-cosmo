/*
 * buffer.h - Text buffer (gap buffer implementation)
 */
#ifndef TEDIT_BUFFER_H
#define TEDIT_BUFFER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Buffer {
    char *data;
    size_t gap_start;
    size_t gap_end;
    size_t capacity;
} Buffer;

Buffer *buffer_create(size_t initial_capacity);
void buffer_destroy(Buffer *buf);

size_t buffer_length(Buffer *buf);
void buffer_insert(Buffer *buf, size_t pos, const char *text, size_t len);
void buffer_delete(Buffer *buf, size_t pos, size_t len);
size_t buffer_get_text(Buffer *buf, char *out, size_t max);
char buffer_char_at(Buffer *buf, size_t pos);

void buffer_clear(Buffer *buf);

#ifdef __cplusplus
}
#endif

#endif /* TEDIT_BUFFER_H */

