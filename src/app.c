/*
 * app.c - Application state management
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <strings.h>  /* strcasecmp on Unix */
#endif

#include "app.h"
#include "util.h"

/* Binary file extensions */
static const char *binary_extensions[] = {
    ".exe", ".com", ".dll", ".so", ".o", ".obj", ".a", ".lib",
    ".elf", ".bin", ".dylib", ".ape", NULL
};

/* Cross-platform case-insensitive compare */
#ifdef _WIN32
#define STRCASECMP _stricmp
#else
#define STRCASECMP strcasecmp
#endif

int app_is_binary_file(const char *path) {
    const char *ext = path_extension(path);
    if (!ext) return 0;
    
    for (int i = 0; binary_extensions[i]; i++) {
        if (STRCASECMP(ext, binary_extensions[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

int app_init(AppState *app) {
    memset(app, 0, sizeof(*app));
    
    app->editor_capacity = 8;
    app->editors = calloc(app->editor_capacity, sizeof(EditorState *));
    if (!app->editors) return -1;
    
    app->disasm_capacity = 4;
    app->disasm_views = calloc(app->disasm_capacity, sizeof(DisasmView *));
    if (!app->disasm_views) {
        free(app->editors);
        return -1;
    }
    
    app->tab_capacity = 16;
    app->tabs = calloc(app->tab_capacity, sizeof(ViewTab));
    if (!app->tabs) {
        free(app->disasm_views);
        free(app->editors);
        return -1;
    }
    
    config_defaults(&app->config);
    build_config_defaults(&app->build);
    
    /* Create initial empty editor */
    if (!app_new_editor(app)) {
        free(app->tabs);
        free(app->disasm_views);
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
    for (size_t i = 0; i < app->disasm_count; i++) {
        if (app->disasm_views[i]) {
            disasm_view_destroy(app->disasm_views[i]);
        }
    }
    free(app->editors);
    free(app->disasm_views);
    free(app->tabs);
    menu_free(&app->menus);
}

static int add_tab(AppState *app, ViewType type, void *view, const char *title) {
    if (app->tab_count >= app->tab_capacity) {
        size_t new_cap = app->tab_capacity * 2;
        ViewTab *new_tabs = realloc(app->tabs, new_cap * sizeof(ViewTab));
        if (!new_tabs) return -1;
        app->tabs = new_tabs;
        app->tab_capacity = new_cap;
    }
    
    ViewTab *tab = &app->tabs[app->tab_count++];
    tab->type = type;
    if (type == VIEW_EDITOR) {
        tab->editor = view;
    } else {
        tab->disasm = view;
    }
    strncpy(tab->title, title ? title : "Untitled", sizeof(tab->title) - 1);
    app->active_tab = app->tab_count - 1;
    
    return 0;
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
    
    /* Add tab for this editor */
    add_tab(app, VIEW_EDITOR, ed, "Untitled");
    
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
    /* Check if this is a binary file - route to disasm view */
    if (app_is_binary_file(path)) {
        return app_open_binary(app, path);
    }
    
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
    
    /* Update tab title */
    ViewTab *tab = app_get_active_tab(app);
    if (tab) {
        strncpy(tab->title, path_basename(path), sizeof(tab->title) - 1);
    }
    
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

/* ============================================================================
 * Disassembly View Management
 * ============================================================================ */

DisasmView *app_new_disasm_view(AppState *app) {
    if (app->disasm_count >= app->disasm_capacity) {
        size_t new_cap = app->disasm_capacity * 2;
        DisasmView **new_views = realloc(app->disasm_views, new_cap * sizeof(DisasmView *));
        if (!new_views) return NULL;
        app->disasm_views = new_views;
        app->disasm_capacity = new_cap;
    }
    
    DisasmView *view = disasm_view_create();
    if (!view) return NULL;
    
    app->disasm_views[app->disasm_count++] = view;
    return view;
}

DisasmView *app_get_active_disasm(AppState *app) {
    ViewTab *tab = app_get_active_tab(app);
    if (tab && tab->type == VIEW_DISASM) {
        return tab->disasm;
    }
    return NULL;
}

int app_open_binary(AppState *app, const char *path) {
    DisasmView *view = app_new_disasm_view(app);
    if (!view) {
        fprintf(stderr, "Failed to create disasm view\n");
        return -1;
    }
    
    if (disasm_view_load_file(view, path) != 0) {
        fprintf(stderr, "Failed to load binary: %s\n", path);
        /* Remove the view */
        app->disasm_count--;
        disasm_view_destroy(view);
        return -1;
    }
    
    /* Add tab for this view */
    add_tab(app, VIEW_DISASM, view, path_basename(path));
    
    return 0;
}

int app_close_disasm_view(AppState *app, size_t index) {
    if (index >= app->disasm_count) return -1;
    
    disasm_view_destroy(app->disasm_views[index]);
    
    /* Shift remaining views */
    for (size_t i = index; i < app->disasm_count - 1; i++) {
        app->disasm_views[i] = app->disasm_views[i + 1];
    }
    app->disasm_count--;
    
    return 0;
}

/* ============================================================================
 * Tab Management
 * ============================================================================ */

ViewTab *app_get_active_tab(AppState *app) {
    if (app->tab_count == 0) return NULL;
    if (app->active_tab >= app->tab_count) {
        app->active_tab = app->tab_count - 1;
    }
    return &app->tabs[app->active_tab];
}

int app_switch_tab(AppState *app, size_t index) {
    if (index >= app->tab_count) return -1;
    app->active_tab = index;
    return 0;
}

int app_close_tab(AppState *app, size_t index) {
    if (index >= app->tab_count) return -1;
    
    ViewTab *tab = &app->tabs[index];
    
    /* Close the underlying view */
    if (tab->type == VIEW_EDITOR) {
        /* Find and close the editor */
        for (size_t i = 0; i < app->editor_count; i++) {
            if (app->editors[i] == tab->editor) {
                app_close_editor(app, i);
                break;
            }
        }
    } else if (tab->type == VIEW_DISASM) {
        /* Find and close the disasm view */
        for (size_t i = 0; i < app->disasm_count; i++) {
            if (app->disasm_views[i] == tab->disasm) {
                app_close_disasm_view(app, i);
                break;
            }
        }
    }
    
    /* Shift remaining tabs */
    for (size_t i = index; i < app->tab_count - 1; i++) {
        app->tabs[i] = app->tabs[i + 1];
    }
    app->tab_count--;
    
    if (app->active_tab >= app->tab_count && app->tab_count > 0) {
        app->active_tab = app->tab_count - 1;
    }
    
    return 0;
}

