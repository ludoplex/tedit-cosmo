/*
 * util.c - Utility functions
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "util.h"

char *str_trim(char *s) {
    if (!s) return s;
    
    /* Trim leading */
    while (isspace((unsigned char)*s)) s++;
    
    if (*s == '\0') return s;
    
    /* Trim trailing */
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) {
        *end-- = '\0';
    }
    
    return s;
}

char *str_dup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *d = malloc(len + 1);
    if (d) strcpy(d, s);
    return d;
}

int str_ends_with(const char *s, const char *suffix) {
    if (!s || !suffix) return 0;
    size_t slen = strlen(s);
    size_t sufflen = strlen(suffix);
    if (sufflen > slen) return 0;
    return strcmp(s + slen - sufflen, suffix) == 0;
}

int str_starts_with(const char *s, const char *prefix) {
    if (!s || !prefix) return 0;
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

const char *path_basename(const char *path) {
    if (!path) return NULL;
    
    const char *last_sep = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') {
            last_sep = p + 1;
        }
    }
    return last_sep;
}

const char *path_extension(const char *path) {
    if (!path) return NULL;
    
    const char *base = path_basename(path);
    const char *dot = strrchr(base, '.');
    return dot ? dot : "";
}

void path_dirname(const char *path, char *dir, size_t max) {
    if (!path || !dir || max == 0) return;
    
    const char *last_sep = NULL;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') {
            last_sep = p;
        }
    }
    
    if (last_sep) {
        size_t len = last_sep - path;
        if (len >= max) len = max - 1;
        memcpy(dir, path, len);
        dir[len] = '\0';
    } else {
        dir[0] = '.';
        dir[1] = '\0';
    }
}

void path_join(char *out, size_t max, const char *a, const char *b) {
    if (!out || max == 0) return;
    
    size_t alen = a ? strlen(a) : 0;
    if (alen > 0 && alen < max - 1) {
        strcpy(out, a);
        if (a[alen-1] != '/' && a[alen-1] != '\\') {
            if (alen + 1 < max) {
                out[alen++] = '/';
                out[alen] = '\0';
            }
        }
    } else {
        out[0] = '\0';
        alen = 0;
    }
    
    if (b && alen < max) {
        strncat(out, b, max - alen - 1);
    }
}

