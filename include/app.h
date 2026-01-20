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

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AppState {
    EditorState **editors;
    size_t editor_count;
    size_t editor_capacity;
    size_t active_editor;
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

#ifdef __cplusplus
}
#endif

#endif /* TEDIT_APP_H */

