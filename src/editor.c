/*
 * editor.c - Editor operations
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
    
    return ed;
}

void editor_destroy(EditorState *ed) {
    if (ed) {
        buffer_destroy(ed->buffer);
        free(ed);
    }
}

int editor_set_text(EditorState *ed, const char *text, size_t len) {
    buffer_clear(ed->buffer);
    buffer_insert(ed->buffer, 0, text, len);
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
    buffer_insert(ed->buffer, pos, text, len);
    ed->dirty = 1;
}

void editor_delete(EditorState *ed, size_t pos, size_t len) {
    buffer_delete(ed->buffer, pos, len);
    ed->dirty = 1;
}

void editor_undo(EditorState *ed) {
    /* TODO: Implement undo stack */
    (void)ed;
}

void editor_redo(EditorState *ed) {
    /* TODO: Implement redo stack */
    (void)ed;
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

