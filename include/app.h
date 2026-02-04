/*
 * app.h - Application state
 */
#ifndef TEDIT_APP_H
#define TEDIT_APP_H

#include <stddef.h>
#include "config.h"
#include "editor.h"
#include "menu.h"
#include "build.h"
#include "disasm_view.h"

#ifdef __cplusplus
extern "C" {
#endif

/* View types for tabs */
typedef enum {
    VIEW_EDITOR = 0,
    VIEW_DISASM = 1,
} ViewType;

/* Active view union */
typedef struct {
    ViewType type;
    union {
        EditorState *editor;
        DisasmView *disasm;
    };
    char title[128];
} ViewTab;

typedef struct AppState {
    EditorState **editors;
    size_t editor_count;
    size_t editor_capacity;
    size_t active_editor;
    
    /* Disassembly views */
    DisasmView **disasm_views;
    size_t disasm_count;
    size_t disasm_capacity;
    
    /* Unified tab system */
    ViewTab *tabs;
    size_t tab_count;
    size_t tab_capacity;
    size_t active_tab;
    
    Config config;
    BuildConfig build;
    MenuSet menus;
    char exe_dir[260];
    int running;
    int gui_mode;
} AppState;

int app_init(AppState *app);
void app_shutdown(AppState *app);

EditorState *app_new_editor(AppState *app);
EditorState *app_get_active_editor(AppState *app);
int app_close_editor(AppState *app, size_t index);

int app_open_file(AppState *app, const char *path);
int app_save_file(AppState *app, const char *path);

/* Disassembly view management */
DisasmView *app_new_disasm_view(AppState *app);
DisasmView *app_get_active_disasm(AppState *app);
int app_open_binary(AppState *app, const char *path);
int app_close_disasm_view(AppState *app, size_t index);

/* Tab management */
ViewTab *app_get_active_tab(AppState *app);
int app_switch_tab(AppState *app, size_t index);
int app_close_tab(AppState *app, size_t index);

/* File type detection */
int app_is_binary_file(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* TEDIT_APP_H */

