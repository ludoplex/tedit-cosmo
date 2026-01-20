/*
 * buffer.c - Gap buffer implementation
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buffer.h"

#define GAP_SIZE 1024

Buffer *buffer_create(size_t initial_capacity) {
    Buffer *buf = malloc(sizeof(Buffer));
    if (!buf) return NULL;
    
    if (initial_capacity < GAP_SIZE) {
        initial_capacity = GAP_SIZE;
    }
    
    buf->data = malloc(initial_capacity);
    if (!buf->data) {
        free(buf);
        return NULL;
    }
    
    buf->capacity = initial_capacity;
    buf->gap_start = 0;
    buf->gap_end = initial_capacity;
    
    return buf;
}

void buffer_destroy(Buffer *buf) {
    if (buf) {
        free(buf->data);
        free(buf);
    }
}

size_t buffer_length(Buffer *buf) {
    return buf->capacity - (buf->gap_end - buf->gap_start);
}

static void buffer_move_gap(Buffer *buf, size_t pos) {
    if (pos == buf->gap_start) return;
    
    size_t gap_size = buf->gap_end - buf->gap_start;
    
    if (pos < buf->gap_start) {
        size_t move_size = buf->gap_start - pos;
        memmove(buf->data + buf->gap_end - move_size,
                buf->data + pos, move_size);
        buf->gap_start = pos;
        buf->gap_end = pos + gap_size;
    } else {
        size_t move_size = pos - buf->gap_start;
        memmove(buf->data + buf->gap_start,
                buf->data + buf->gap_end, move_size);
        buf->gap_start = pos;
        buf->gap_end = pos + gap_size;
    }
}

static void buffer_grow(Buffer *buf, size_t needed) {
    size_t gap_size = buf->gap_end - buf->gap_start;
    if (gap_size >= needed) return;
    
    size_t new_cap = buf->capacity + needed + GAP_SIZE;
    char *new_data = malloc(new_cap);
    if (!new_data) return;
    
    /* Copy before gap */
    memcpy(new_data, buf->data, buf->gap_start);
    
    /* Copy after gap */
    size_t after_gap = buf->capacity - buf->gap_end;
    memcpy(new_data + new_cap - after_gap,
           buf->data + buf->gap_end, after_gap);
    
    free(buf->data);
    buf->data = new_data;
    buf->gap_end = new_cap - after_gap;
    buf->capacity = new_cap;
}

void buffer_insert(Buffer *buf, size_t pos, const char *text, size_t len) {
    if (pos > buffer_length(buf)) {
        pos = buffer_length(buf);
    }
    
    buffer_grow(buf, len);
    buffer_move_gap(buf, pos);
    
    memcpy(buf->data + buf->gap_start, text, len);
    buf->gap_start += len;
}

void buffer_delete(Buffer *buf, size_t pos, size_t len) {
    size_t buf_len = buffer_length(buf);
    if (pos >= buf_len) return;
    if (pos + len > buf_len) {
        len = buf_len - pos;
    }
    
    buffer_move_gap(buf, pos);
    buf->gap_end += len;
}

size_t buffer_get_text(Buffer *buf, char *out, size_t max) {
    size_t len = buffer_length(buf);
    if (len >= max) len = max - 1;
    
    size_t before = buf->gap_start;
    size_t after = buf->capacity - buf->gap_end;
    
    if (before > len) before = len;
    memcpy(out, buf->data, before);
    
    if (before < len) {
        size_t remaining = len - before;
        if (remaining > after) remaining = after;
        memcpy(out + before, buf->data + buf->gap_end, remaining);
    }
    
    out[len] = '\0';
    return len;
}

char buffer_char_at(Buffer *buf, size_t pos) {
    if (pos >= buffer_length(buf)) return '\0';
    
    if (pos < buf->gap_start) {
        return buf->data[pos];
    } else {
        return buf->data[buf->gap_end + (pos - buf->gap_start)];
    }
}

void buffer_clear(Buffer *buf) {
    buf->gap_start = 0;
    buf->gap_end = buf->capacity;
}

