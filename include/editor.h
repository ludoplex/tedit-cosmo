/*
 * editor.h - Editor state and text operations
 */
#ifndef TEDIT_EDITOR_H
#define TEDIT_EDITOR_H

#include <stddef.h>
#include "buffer.h"
#include "syntax.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EditorState {
    Buffer *buffer;
    char file_path[260];
    size_t cursor_line;
    size_t cursor_col;
    size_t selection_start;
    size_t selection_end;
    Language language;
    int dirty;
    int readonly;
} EditorState;

EditorState *editor_create(void);
void editor_destroy(EditorState *ed);

int editor_set_text(EditorState *ed, const char *text, size_t len);
size_t editor_get_text(EditorState *ed, char *buf, size_t max);
size_t editor_get_length(EditorState *ed);

void editor_set_language(EditorState *ed, Language lang);
Language editor_detect_language(const char *filename);

/* Edit operations */
void editor_insert(EditorState *ed, size_t pos, const char *text, size_t len);
void editor_delete(EditorState *ed, size_t pos, size_t len);
void editor_undo(EditorState *ed);
void editor_redo(EditorState *ed);

/* Selection */
void editor_select_all(EditorState *ed);
char *editor_get_selection(EditorState *ed, size_t *len);

/* Cursor */
void editor_goto_line(EditorState *ed, size_t line);
void editor_get_cursor_pos(EditorState *ed, size_t *line, size_t *col);

#ifdef __cplusplus
}
#endif

#endif /* TEDIT_EDITOR_H */

