/*
 * history.h - Write-through operation history for persistent undo/redo
 * 
 * Every edit operation is appended to a .tedit-history file immediately,
 * providing crash-proof, cross-session undo/redo capability.
 */
#ifndef TEDIT_HISTORY_H
#define TEDIT_HISTORY_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* History file magic and version */
#define HISTORY_MAGIC    "THIST001"
#define HISTORY_VERSION  1

/* Operation types */
typedef enum {
    OP_INSERT = 1,
    OP_DELETE = 2
} OpType;

/* Single edit operation */
typedef struct EditOp {
    OpType type;
    uint32_t position;      /* Byte offset in document */
    uint32_t length;        /* Length of data */
    uint64_t timestamp;     /* Unix timestamp (ms) */
    char *data;             /* Content (for INSERT: new text, DELETE: removed text) */
    struct EditOp *next;    /* Linked list for in-memory ops */
    struct EditOp *prev;
} EditOp;

/* History file header (32 bytes, packed) */
#pragma pack(push, 1)
typedef struct HistoryHeader {
    char magic[8];          /* "THIST001" */
    uint32_t version;       /* File format version */
    uint64_t created;       /* Creation timestamp */
    uint32_t flags;         /* Reserved flags */
    uint64_t reserved;      /* Future use */
} HistoryHeader;
#pragma pack(pop)

/* History manager */
typedef struct History {
    char file_path[260];        /* Path to source file */
    char history_path[268];     /* Path to .tedit-history file */
    FILE *file;                 /* Open history file handle */
    
    EditOp *ops_head;           /* Linked list of operations */
    EditOp *ops_tail;
    EditOp *current;            /* Current position for undo/redo */
    
    size_t op_count;            /* Total operations */
    size_t file_size;           /* History file size in bytes */
    
    int dirty;                  /* Has unsaved ops in memory */
} History;

/* Create/Open/Close */
History *history_open(const char *file_path);
void history_close(History *h);

/* Append operation (called on every edit) - writes immediately to disk */
int history_append(History *h, OpType type, size_t pos, 
                   const char *data, size_t len);

/* Undo/Redo - returns the operation to apply (caller must apply to buffer) */
EditOp *history_undo(History *h);
EditOp *history_redo(History *h);

/* Check if undo/redo is available */
int history_can_undo(History *h);
int history_can_redo(History *h);

/* Get history file size */
size_t history_size(History *h);

/* Get operation count */
size_t history_count(History *h);

/* Compact history - merge operations and archive old ones */
int history_compact(History *h, const char *archive_path);

/* Trim history - remove operations before a given time */
int history_trim(History *h, time_t before);

/* Clear all history */
int history_clear(History *h);

/* Export history to human-readable format */
int history_export(History *h, const char *output_path);

/* Reload history from disk (after external changes) */
int history_reload(History *h);

/* Utility: get history path for a file */
void history_get_path(const char *file_path, char *out, size_t out_size);

/* Utility: get timestamp in milliseconds */
uint64_t history_get_timestamp(void);

#ifdef __cplusplus
}
#endif

#endif /* TEDIT_HISTORY_H */


