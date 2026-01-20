/*
 * file.c - File I/O operations
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

char *file_read_all(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    
    char *data = malloc(size + 1);
    if (!data) {
        fclose(f);
        return NULL;
    }
    
    size_t read = fread(data, 1, size, f);
    fclose(f);
    
    data[read] = '\0';
    if (len) *len = read;
    
    return data;
}

int file_write_all(const char *path, const char *data, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    
    size_t written = fwrite(data, 1, len, f);
    fclose(f);
    
    return (written == len) ? 0 : -1;
}

int file_exists(const char *path) {
    FILE *f = fopen(path, "r");
    if (f) {
        fclose(f);
        return 1;
    }
    return 0;
}

