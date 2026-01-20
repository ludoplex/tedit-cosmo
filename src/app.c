/*
 * app.c - Application state management
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "util.h"

int app_init(AppState *app) {
    memset(app, 0, sizeof(*app));
    
    app->editor_capacity = 8;
    app->editors = calloc(app->editor_capacity, sizeof(EditorState *));
    if (!app->editors) return -1;
    
    config_defaults(&app->config);
    build_config_defaults(&app->build);
    
    /* Create initial empty editor */
    if (!app_new_editor(app)) {
        free(app->editors);
        return -1;
    }
    
    app->running = 1;
    return 0;
}

void app_shutdown(AppState *app) {
    for (size_t i = 0; i < app->editor_count; i++) {
        if (app->editors[i]) {
            editor_destroy(app->editors[i]);
        }
    }
    free(app->editors);
    menu_free(&app->menus);
}

EditorState *app_new_editor(AppState *app) {
    if (app->editor_count >= app->editor_capacity) {
        size_t new_cap = app->editor_capacity * 2;
        EditorState **new_eds = realloc(app->editors, new_cap * sizeof(EditorState *));
        if (!new_eds) return NULL;
        app->editors = new_eds;
        app->editor_capacity = new_cap;
    }
    
    EditorState *ed = editor_create();
    if (!ed) return NULL;
    
    app->editors[app->editor_count++] = ed;
    app->active_editor = app->editor_count - 1;
    return ed;
}

EditorState *app_get_active_editor(AppState *app) {
    if (app->editor_count == 0) return NULL;
    if (app->active_editor >= app->editor_count) {
        app->active_editor = app->editor_count - 1;
    }
    return app->editors[app->active_editor];
}

int app_close_editor(AppState *app, size_t index) {
    if (index >= app->editor_count) return -1;
    
    editor_destroy(app->editors[index]);
    
    /* Shift remaining editors */
    for (size_t i = index; i < app->editor_count - 1; i++) {
        app->editors[i] = app->editors[i + 1];
    }
    app->editor_count--;
    
    if (app->active_editor >= app->editor_count && app->editor_count > 0) {
        app->active_editor = app->editor_count - 1;
    }
    
    return 0;
}

int app_open_file(AppState *app, const char *path) {
    size_t len;
    char *content = file_read_all(path, &len);
    if (!content) {
        fprintf(stderr, "Failed to open: %s\n", path);
        return -1;
    }
    
    EditorState *ed = app_get_active_editor(app);
    if (!ed) {
        ed = app_new_editor(app);
        if (!ed) {
            free(content);
            return -1;
        }
    }
    
    editor_set_text(ed, content, len);
    strncpy(ed->file_path, path, sizeof(ed->file_path) - 1);
    ed->language = editor_detect_language(path);
    ed->dirty = 0;
    
    config_add_recent(&app->config, path);
    free(content);
    return 0;
}

int app_save_file(AppState *app, const char *path) {
    EditorState *ed = app_get_active_editor(app);
    if (!ed) return -1;
    
    size_t len = editor_get_length(ed);
    char *content = malloc(len + 1);
    if (!content) return -1;
    
    editor_get_text(ed, content, len + 1);
    
    if (file_write_all(path, content, len) != 0) {
        free(content);
        return -1;
    }
    
    strncpy(ed->file_path, path, sizeof(ed->file_path) - 1);
    ed->dirty = 0;
    
    free(content);
    return 0;
}

