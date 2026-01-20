/*
 * editor.c - Editor operations with integrated history
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "editor.h"
#include "util.h"

EditorState *editor_create(void) {
    EditorState *ed = calloc(1, sizeof(EditorState));
    if (!ed) return NULL;
    
    ed->buffer = buffer_create(4096);
    if (!ed->buffer) {
        free(ed);
        return NULL;
    }
    
    ed->language = LANG_NONE;
    ed->cursor_line = 1;
    ed->cursor_col = 1;
    ed->history = NULL;
    ed->history_enabled = 1;  /* Enable by default */
    
    return ed;
}

void editor_destroy(EditorState *ed) {
    if (ed) {
        buffer_destroy(ed->buffer);
        if (ed->history) {
            history_close(ed->history);
        }
        free(ed);
    }
}

int editor_set_text(EditorState *ed, const char *text, size_t len) {
    buffer_clear(ed->buffer);
    
    /* Insert without recording to history (bulk load) */
    int prev_enabled = ed->history_enabled;
    ed->history_enabled = 0;
    buffer_insert(ed->buffer, 0, text, len);
    ed->history_enabled = prev_enabled;
    
    ed->dirty = 1;
    ed->cursor_line = 1;
    ed->cursor_col = 1;
    return 0;
}

size_t editor_get_text(EditorState *ed, char *buf, size_t max) {
    return buffer_get_text(ed->buffer, buf, max);
}

size_t editor_get_length(EditorState *ed) {
    return buffer_length(ed->buffer);
}

void editor_set_language(EditorState *ed, Language lang) {
    ed->language = lang;
}

Language editor_detect_language(const char *filename) {
    return syntax_detect_language(filename);
}

void editor_insert(EditorState *ed, size_t pos, const char *text, size_t len) {
    /* Record to history before modifying buffer */
    if (ed->history_enabled && ed->history) {
        history_append(ed->history, OP_INSERT, pos, text, len);
    }
    
    buffer_insert(ed->buffer, pos, text, len);
    ed->dirty = 1;
}

void editor_delete(EditorState *ed, size_t pos, size_t len) {
    /* Get the text being deleted for history */
    if (ed->history_enabled && ed->history && len > 0) {
        char *deleted = malloc(len + 1);
        if (deleted) {
            for (size_t i = 0; i < len; i++) {
                deleted[i] = buffer_char_at(ed->buffer, pos + i);
            }
            deleted[len] = '\0';
            history_append(ed->history, OP_DELETE, pos, deleted, len);
            free(deleted);
        }
    }
    
    buffer_delete(ed->buffer, pos, len);
    ed->dirty = 1;
}

void editor_undo(EditorState *ed) {
    if (!ed->history || !history_can_undo(ed->history)) return;
    
    EditOp *op = history_undo(ed->history);
    if (!op) return;
    
    /* Disable history recording while applying undo */
    int prev_enabled = ed->history_enabled;
    ed->history_enabled = 0;
    
    /* Reverse the operation */
    if (op->type == OP_INSERT) {
        /* Undo insert = delete */
        buffer_delete(ed->buffer, op->position, op->length);
    } else if (op->type == OP_DELETE) {
        /* Undo delete = insert */
        buffer_insert(ed->buffer, op->position, op->data, op->length);
    }
    
    ed->history_enabled = prev_enabled;
    ed->dirty = 1;
}

void editor_redo(EditorState *ed) {
    if (!ed->history || !history_can_redo(ed->history)) return;
    
    EditOp *op = history_redo(ed->history);
    if (!op) return;
    
    /* Disable history recording while applying redo */
    int prev_enabled = ed->history_enabled;
    ed->history_enabled = 0;
    
    /* Reapply the operation */
    if (op->type == OP_INSERT) {
        buffer_insert(ed->buffer, op->position, op->data, op->length);
    } else if (op->type == OP_DELETE) {
        buffer_delete(ed->buffer, op->position, op->length);
    }
    
    ed->history_enabled = prev_enabled;
    ed->dirty = 1;
}

void editor_select_all(EditorState *ed) {
    ed->selection_start = 0;
    ed->selection_end = buffer_length(ed->buffer);
}

char *editor_get_selection(EditorState *ed, size_t *len) {
    if (ed->selection_start >= ed->selection_end) {
        *len = 0;
        return NULL;
    }
    
    *len = ed->selection_end - ed->selection_start;
    char *sel = malloc(*len + 1);
    if (!sel) return NULL;
    
    for (size_t i = 0; i < *len; i++) {
        sel[i] = buffer_char_at(ed->buffer, ed->selection_start + i);
    }
    sel[*len] = '\0';
    
    return sel;
}

void editor_goto_line(EditorState *ed, size_t line) {
    if (line < 1) line = 1;
    
    size_t len = buffer_length(ed->buffer);
    size_t current_line = 1;
    size_t pos = 0;
    
    while (pos < len && current_line < line) {
        if (buffer_char_at(ed->buffer, pos) == '\n') {
            current_line++;
        }
        pos++;
    }
    
    ed->cursor_line = current_line;
    ed->cursor_col = 1;
}

void editor_get_cursor_pos(EditorState *ed, size_t *line, size_t *col) {
    *line = ed->cursor_line;
    *col = ed->cursor_col;
}

/* File operations */
int editor_load_file(EditorState *ed, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    /* Get file size */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size < 0) {
        fclose(f);
        return -1;
    }
    
    /* Read content */
    char *content = malloc(size + 1);
    if (!content) {
        fclose(f);
        return -1;
    }
    
    size_t read = fread(content, 1, size, f);
    content[read] = '\0';
    fclose(f);
    
    /* Set text (without history) */
    editor_set_text(ed, content, read);
    free(content);
    
    /* Store path */
    strncpy(ed->file_path, path, sizeof(ed->file_path) - 1);
    ed->file_path[sizeof(ed->file_path) - 1] = '\0';
    
    /* Detect language */
    ed->language = editor_detect_language(path);
    
    /* Open/create history file */
    if (ed->history) {
        history_close(ed->history);
    }
    ed->history = history_open(path);
    
    ed->dirty = 0;
    
    return 0;
}

int editor_save_file(EditorState *ed, const char *path) {
    if (!path) {
        path = ed->file_path;
    }
    if (!path[0]) return -1;
    
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    
    size_t len = buffer_length(ed->buffer);
    char *content = malloc(len + 1);
    if (!content) {
        fclose(f);
        return -1;
    }
    
    buffer_get_text(ed->buffer, content, len + 1);
    fwrite(content, 1, len, f);
    free(content);
    fclose(f);
    
    /* Update path if saving to new location */
    if (path != ed->file_path) {
        strncpy(ed->file_path, path, sizeof(ed->file_path) - 1);
        ed->file_path[sizeof(ed->file_path) - 1] = '\0';
        
        /* Reopen history for new path */
        if (ed->history) {
            history_close(ed->history);
        }
        ed->history = history_open(path);
    }
    
    ed->dirty = 0;
    
    return 0;
}

/* History management */
void editor_enable_history(EditorState *ed, int enable) {
    ed->history_enabled = enable;
}

int editor_has_history(EditorState *ed) {
    return ed->history != NULL;
}

size_t editor_history_size(EditorState *ed) {
    return ed->history ? history_size(ed->history) : 0;
}

int editor_history_compact(EditorState *ed, const char *archive_path) {
    return ed->history ? history_compact(ed->history, archive_path) : -1;
}

int editor_history_export(EditorState *ed, const char *output_path) {
    return ed->history ? history_export(ed->history, output_path) : -1;
}

int editor_history_clear(EditorState *ed) {
    return ed->history ? history_clear(ed->history) : -1;
}
