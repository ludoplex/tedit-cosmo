/*
 * history.c - Write-through operation history implementation
 * 
 * Provides persistent undo/redo by appending every edit operation
 * to a .tedit-history file immediately upon execution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "history.h"

#ifdef _WIN32
#include <sys/timeb.h>
#else
#include <sys/time.h>
#endif

/* Get current timestamp in milliseconds */
uint64_t history_get_timestamp(void) {
#ifdef _WIN32
    struct _timeb tb;
    _ftime(&tb);
    return (uint64_t)tb.time * 1000 + tb.millitm;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

/* Generate history file path from source file path */
void history_get_path(const char *file_path, char *out, size_t out_size) {
    size_t len = strlen(file_path);
    if (len + 15 >= out_size) {
        out[0] = '\0';
        return;
    }
    snprintf(out, out_size, "%s.tedit-history", file_path);
}

/* Create a new EditOp */
static EditOp *editop_create(OpType type, uint32_t pos, 
                             const char *data, uint32_t len) {
    EditOp *op = calloc(1, sizeof(EditOp));
    if (!op) return NULL;
    
    op->type = type;
    op->position = pos;
    op->length = len;
    op->timestamp = history_get_timestamp();
    
    if (len > 0 && data) {
        op->data = malloc(len + 1);
        if (!op->data) {
            free(op);
            return NULL;
        }
        memcpy(op->data, data, len);
        op->data[len] = '\0';
    }
    
    return op;
}

/* Free an EditOp */
static void editop_destroy(EditOp *op) {
    if (op) {
        free(op->data);
        free(op);
    }
}

/* Write history header to file */
static int history_write_header(History *h) {
    if (!h->file) return -1;
    
    HistoryHeader header;
    memset(&header, 0, sizeof(header));
    memcpy(header.magic, HISTORY_MAGIC, 8);
    header.version = HISTORY_VERSION;
    header.created = history_get_timestamp();
    header.flags = 0;
    header.reserved = 0;
    
    fseek(h->file, 0, SEEK_SET);
    if (fwrite(&header, sizeof(header), 1, h->file) != 1) {
        return -1;
    }
    fflush(h->file);
    
    h->file_size = sizeof(header);
    return 0;
}

/* Read and validate history header */
static int history_read_header(History *h, HistoryHeader *header) {
    if (!h->file) return -1;
    
    fseek(h->file, 0, SEEK_SET);
    if (fread(header, sizeof(*header), 1, h->file) != 1) {
        return -1;
    }
    
    if (memcmp(header->magic, HISTORY_MAGIC, 8) != 0) {
        return -1;  /* Invalid magic */
    }
    
    if (header->version > HISTORY_VERSION) {
        return -1;  /* Unsupported version */
    }
    
    return 0;
}

/* Write a single operation to the history file */
static int history_write_op(History *h, EditOp *op) {
    if (!h->file || !op) return -1;
    
    /* Seek to end */
    fseek(h->file, 0, SEEK_END);
    
    /* Write operation fields */
    uint8_t type = (uint8_t)op->type;
    if (fwrite(&type, sizeof(type), 1, h->file) != 1) return -1;
    if (fwrite(&op->position, sizeof(op->position), 1, h->file) != 1) return -1;
    if (fwrite(&op->length, sizeof(op->length), 1, h->file) != 1) return -1;
    if (fwrite(&op->timestamp, sizeof(op->timestamp), 1, h->file) != 1) return -1;
    
    /* Write data if present */
    if (op->length > 0 && op->data) {
        if (fwrite(op->data, 1, op->length, h->file) != op->length) return -1;
    }
    
    fflush(h->file);
    
    /* Update file size */
    h->file_size += 1 + 4 + 4 + 8 + op->length;
    
    return 0;
}

/* Read a single operation from the history file */
static EditOp *history_read_op(History *h) {
    if (!h->file) return NULL;
    
    uint8_t type;
    uint32_t position, length;
    uint64_t timestamp;
    
    if (fread(&type, sizeof(type), 1, h->file) != 1) return NULL;
    if (fread(&position, sizeof(position), 1, h->file) != 1) return NULL;
    if (fread(&length, sizeof(length), 1, h->file) != 1) return NULL;
    if (fread(&timestamp, sizeof(timestamp), 1, h->file) != 1) return NULL;
    
    char *data = NULL;
    if (length > 0) {
        data = malloc(length + 1);
        if (!data) return NULL;
        if (fread(data, 1, length, h->file) != length) {
            free(data);
            return NULL;
        }
        data[length] = '\0';
    }
    
    EditOp *op = calloc(1, sizeof(EditOp));
    if (!op) {
        free(data);
        return NULL;
    }
    
    op->type = (OpType)type;
    op->position = position;
    op->length = length;
    op->timestamp = timestamp;
    op->data = data;
    
    return op;
}

/* Load all operations from history file */
static int history_load_ops(History *h) {
    if (!h->file) return -1;
    
    HistoryHeader header;
    if (history_read_header(h, &header) != 0) {
        return -1;
    }
    
    /* Read all operations */
    while (!feof(h->file)) {
        long pos = ftell(h->file);
        EditOp *op = history_read_op(h);
        if (!op) break;
        
        /* Add to linked list */
        op->prev = h->ops_tail;
        op->next = NULL;
        
        if (h->ops_tail) {
            h->ops_tail->next = op;
        } else {
            h->ops_head = op;
        }
        h->ops_tail = op;
        h->op_count++;
    }
    
    /* Current points past the last op (ready for new ops) */
    h->current = NULL;
    
    return 0;
}

/* Open or create history for a file */
History *history_open(const char *file_path) {
    History *h = calloc(1, sizeof(History));
    if (!h) return NULL;
    
    strncpy(h->file_path, file_path, sizeof(h->file_path) - 1);
    history_get_path(file_path, h->history_path, sizeof(h->history_path));
    
    /* Try to open existing history file */
    h->file = fopen(h->history_path, "r+b");
    if (h->file) {
        /* Get file size */
        fseek(h->file, 0, SEEK_END);
        h->file_size = ftell(h->file);
        fseek(h->file, 0, SEEK_SET);
        
        /* Load existing operations */
        if (history_load_ops(h) != 0) {
            /* Invalid or corrupted history - create new */
            fclose(h->file);
            h->file = NULL;
        }
    }
    
    /* Create new history file if needed */
    if (!h->file) {
        h->file = fopen(h->history_path, "w+b");
        if (!h->file) {
            free(h);
            return NULL;
        }
        
        if (history_write_header(h) != 0) {
            fclose(h->file);
            free(h);
            return NULL;
        }
    }
    
    return h;
}

/* Close history and free resources */
void history_close(History *h) {
    if (!h) return;
    
    /* Free all operations */
    EditOp *op = h->ops_head;
    while (op) {
        EditOp *next = op->next;
        editop_destroy(op);
        op = next;
    }
    
    /* Close file */
    if (h->file) {
        fclose(h->file);
    }
    
    free(h);
}

/* Append a new operation (write-through) */
int history_append(History *h, OpType type, size_t pos, 
                   const char *data, size_t len) {
    if (!h) return -1;
    
    /* Create operation */
    EditOp *op = editop_create(type, (uint32_t)pos, data, (uint32_t)len);
    if (!op) return -1;
    
    /* If we've undone some operations, discard the redo chain */
    if (h->current) {
        /* current points to the last undone op, discard everything after current */
        EditOp *to_discard = h->current->next;
        h->current->next = NULL;
        h->ops_tail = h->current;
        
        while (to_discard) {
            EditOp *next = to_discard->next;
            editop_destroy(to_discard);
            h->op_count--;
            to_discard = next;
        }
        
        h->current = NULL;
    }
    
    /* Add to linked list */
    op->prev = h->ops_tail;
    op->next = NULL;
    
    if (h->ops_tail) {
        h->ops_tail->next = op;
    } else {
        h->ops_head = op;
    }
    h->ops_tail = op;
    h->op_count++;
    
    /* Write immediately to disk (write-through) */
    if (history_write_op(h, op) != 0) {
        return -1;
    }
    
    return 0;
}

/* Undo - returns the operation to reverse */
EditOp *history_undo(History *h) {
    if (!h || !history_can_undo(h)) return NULL;
    
    EditOp *op;
    if (h->current == NULL) {
        /* First undo - start from tail */
        op = h->ops_tail;
    } else {
        /* Continue undoing from current position */
        op = h->current->prev;
    }
    
    if (op) {
        h->current = op;
    }
    
    return op;
}

/* Redo - returns the operation to reapply */
EditOp *history_redo(History *h) {
    if (!h || !history_can_redo(h)) return NULL;
    
    EditOp *op;
    if (h->current == NULL) {
        /* Nothing to redo - we're at the end */
        return NULL;
    }
    
    op = h->current;
    h->current = h->current->next ? h->current->next : NULL;
    
    /* If current becomes NULL, we're back at the end */
    if (h->current == NULL && op->next) {
        h->current = op->next;
        op = h->current;
        h->current = h->current->next;
    }
    
    return op;
}

/* Check if undo is available */
int history_can_undo(History *h) {
    if (!h) return 0;
    
    if (h->current == NULL) {
        /* Can undo if there are any ops */
        return h->ops_tail != NULL;
    }
    
    /* Can undo if current has a previous */
    return h->current->prev != NULL;
}

/* Check if redo is available */
int history_can_redo(History *h) {
    if (!h) return 0;
    
    /* Can redo if current is not NULL and not at the end */
    return h->current != NULL;
}

/* Get history file size */
size_t history_size(History *h) {
    return h ? h->file_size : 0;
}

/* Get operation count */
size_t history_count(History *h) {
    return h ? h->op_count : 0;
}

/* Clear all history */
int history_clear(History *h) {
    if (!h) return -1;
    
    /* Free all operations */
    EditOp *op = h->ops_head;
    while (op) {
        EditOp *next = op->next;
        editop_destroy(op);
        op = next;
    }
    
    h->ops_head = NULL;
    h->ops_tail = NULL;
    h->current = NULL;
    h->op_count = 0;
    
    /* Truncate and rewrite header */
    if (h->file) {
        freopen(h->history_path, "w+b", h->file);
        if (h->file) {
            history_write_header(h);
        }
    }
    
    return 0;
}

/* Compact history - merge consecutive operations */
int history_compact(History *h, const char *archive_path) {
    if (!h) return -1;
    
    /* Archive current history if path provided */
    if (archive_path && archive_path[0]) {
        FILE *archive = fopen(archive_path, "wb");
        if (archive) {
            /* Copy current history file */
            fseek(h->file, 0, SEEK_SET);
            char buf[4096];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), h->file)) > 0) {
                fwrite(buf, 1, n, archive);
            }
            fclose(archive);
        }
    }
    
    /* For now, just clear the history */
    /* TODO: Implement smart merging of consecutive ops */
    return history_clear(h);
}

/* Trim history before a given time */
int history_trim(History *h, time_t before) {
    if (!h) return -1;
    
    uint64_t before_ms = (uint64_t)before * 1000;
    
    /* Remove old operations from the head */
    while (h->ops_head && h->ops_head->timestamp < before_ms) {
        EditOp *old = h->ops_head;
        h->ops_head = old->next;
        if (h->ops_head) {
            h->ops_head->prev = NULL;
        } else {
            h->ops_tail = NULL;
        }
        
        if (h->current == old) {
            h->current = h->ops_head;
        }
        
        editop_destroy(old);
        h->op_count--;
    }
    
    /* Rewrite the history file */
    if (h->file) {
        freopen(h->history_path, "w+b", h->file);
        if (!h->file) return -1;
        
        history_write_header(h);
        
        EditOp *op = h->ops_head;
        while (op) {
            history_write_op(h, op);
            op = op->next;
        }
    }
    
    return 0;
}

/* Export history to human-readable format */
int history_export(History *h, const char *output_path) {
    if (!h || !output_path) return -1;
    
    FILE *out = fopen(output_path, "w");
    if (!out) return -1;
    
    fprintf(out, "# tedit-cosmo history export\n");
    fprintf(out, "# Source: %s\n", h->file_path);
    fprintf(out, "# Operations: %zu\n", h->op_count);
    fprintf(out, "# File size: %zu bytes\n\n", h->file_size);
    
    EditOp *op = h->ops_head;
    size_t i = 0;
    while (op) {
        time_t ts = (time_t)(op->timestamp / 1000);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&ts));
        
        fprintf(out, "[%zu] %s at pos %u, len %u (%s)\n",
                i++,
                op->type == OP_INSERT ? "INSERT" : "DELETE",
                op->position,
                op->length,
                time_str);
        
        if (op->data && op->length > 0) {
            fprintf(out, "    Data: ");
            for (uint32_t j = 0; j < op->length && j < 50; j++) {
                char c = op->data[j];
                if (c == '\n') fprintf(out, "\\n");
                else if (c == '\r') fprintf(out, "\\r");
                else if (c == '\t') fprintf(out, "\\t");
                else if (c >= 32 && c < 127) fputc(c, out);
                else fprintf(out, "\\x%02x", (unsigned char)c);
            }
            if (op->length > 50) fprintf(out, "...");
            fprintf(out, "\n");
        }
        
        op = op->next;
    }
    
    fclose(out);
    return 0;
}

/* Reload history from disk */
int history_reload(History *h) {
    if (!h) return -1;
    
    /* Free current operations */
    EditOp *op = h->ops_head;
    while (op) {
        EditOp *next = op->next;
        editop_destroy(op);
        op = next;
    }
    
    h->ops_head = NULL;
    h->ops_tail = NULL;
    h->current = NULL;
    h->op_count = 0;
    
    /* Reload from file */
    if (h->file) {
        fseek(h->file, 0, SEEK_END);
        h->file_size = ftell(h->file);
        fseek(h->file, 0, SEEK_SET);
        
        return history_load_ops(h);
    }
    
    return -1;
}


