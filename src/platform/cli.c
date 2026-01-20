/*
 * cli.c - Command-line interface backend
 * 
 * This provides a minimal text-mode interface that works everywhere.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "app.h"
#include "editor.h"
#include "build.h"
#include "menu.h"
#include "util.h"
#include "syntax.h"

static AppState *g_app = NULL;

static void print_help(void) {
    printf("Commands:\n");
    printf("  new                  - Create new file\n");
    printf("  open <path>          - Open file\n");
    printf("  save [path]          - Save file\n");
    printf("  build                - Build current file (cosmocc)\n");
    printf("  run                  - Run built executable\n");
    printf("  buildrun             - Build and run\n");
    printf("  insert <text>        - Insert text at cursor\n");
    printf("  template <file>      - Insert template from textape/\n");
    printf("  show                 - Show buffer contents\n");
    printf("  goto <line>          - Go to line\n");
    printf("  lang <language>      - Set syntax (cosmo|amd64|aarch64|masm64|masm32)\n");
    printf("  menu <ini_path>      - Load menu from INI\n");
    printf("  undo                 - Undo last edit\n");
    printf("  redo                 - Redo last undone edit\n");
    printf("  history              - Show history info\n");
    printf("  history export <out> - Export history to file\n");
    printf("  history clear        - Clear edit history\n");
    printf("  help                 - Show this help\n");
    printf("  quit                 - Exit\n");
}

static void print_status(void) {
    EditorState *ed = app_get_active_editor(g_app);
    if (!ed) {
        printf("[No file]\n");
        return;
    }
    
    size_t line, col;
    editor_get_cursor_pos(ed, &line, &col);
    
    printf("[%s%s] %s | Line %zu, Col %zu | %zu bytes\n",
           ed->file_path[0] ? ed->file_path : "Untitled",
           ed->dirty ? " *" : "",
           syntax_language_name(ed->language),
           line, col, editor_get_length(ed));
}

static void do_build(void) {
    EditorState *ed = app_get_active_editor(g_app);
    if (!ed || !ed->file_path[0]) {
        printf("No file to build. Save first.\n");
        return;
    }
    
    char cmd[1024];
    menu_substitute_vars(cmd, sizeof(cmd), g_app->build.build_cmd,
                         ed->file_path, g_app->exe_dir);
    build_run_command(cmd);
}

static void do_run(void) {
    EditorState *ed = app_get_active_editor(g_app);
    if (!ed || !ed->file_path[0]) {
        printf("No file. Save first.\n");
        return;
    }
    
    char cmd[1024];
    menu_substitute_vars(cmd, sizeof(cmd), g_app->build.run_cmd,
                         ed->file_path, g_app->exe_dir);
    build_run_command(cmd);
}

static void handle_command(const char *line) {
    char cmd[64] = {0};
    char arg[512] = {0};
    
    sscanf(line, "%63s %511[^\n]", cmd, arg);
    str_trim(arg);
    
    EditorState *ed = app_get_active_editor(g_app);
    
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
        print_help();
    }
    else if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0 || strcmp(cmd, "q") == 0) {
        if (ed && ed->dirty) {
            printf("Unsaved changes. Save first or use 'quit!' to discard.\n");
        } else {
            g_app->running = 0;
        }
    }
    else if (strcmp(cmd, "quit!") == 0) {
        g_app->running = 0;
    }
    else if (strcmp(cmd, "new") == 0) {
        app_new_editor(g_app);
        printf("Created new buffer.\n");
    }
    else if (strcmp(cmd, "open") == 0) {
        if (arg[0]) {
            if (app_open_file(g_app, arg) == 0) {
                printf("Opened: %s\n", arg);
            }
        } else {
            printf("Usage: open <path>\n");
        }
    }
    else if (strcmp(cmd, "save") == 0) {
        if (arg[0]) {
            if (app_save_file(g_app, arg) == 0) {
                printf("Saved: %s\n", arg);
            }
        } else if (ed && ed->file_path[0]) {
            if (app_save_file(g_app, ed->file_path) == 0) {
                printf("Saved: %s\n", ed->file_path);
            }
        } else {
            printf("Usage: save <path>\n");
        }
    }
    else if (strcmp(cmd, "build") == 0) {
        do_build();
    }
    else if (strcmp(cmd, "run") == 0) {
        do_run();
    }
    else if (strcmp(cmd, "buildrun") == 0 || strcmp(cmd, "br") == 0) {
        do_build();
        do_run();
    }
    else if (strcmp(cmd, "show") == 0) {
        if (ed) {
            size_t len = editor_get_length(ed);
            char *buf = malloc(len + 1);
            if (buf) {
                editor_get_text(ed, buf, len + 1);
                printf("--- Buffer contents ---\n%s\n--- End ---\n", buf);
                free(buf);
            }
        }
    }
    else if (strcmp(cmd, "insert") == 0) {
        if (ed && arg[0]) {
            size_t pos = 0; /* Insert at start for now */
            editor_insert(ed, pos, arg, strlen(arg));
            editor_insert(ed, pos + strlen(arg), "\n", 1);
        }
    }
    else if (strcmp(cmd, "goto") == 0) {
        if (ed && arg[0]) {
            int line_num = atoi(arg);
            if (line_num > 0) {
                editor_goto_line(ed, line_num);
                printf("Moved to line %d\n", line_num);
            }
        }
    }
    else if (strcmp(cmd, "lang") == 0) {
        if (ed && arg[0]) {
            Language lang = LANG_NONE;
            if (strcmp(arg, "cosmo") == 0 || strcmp(arg, "c") == 0) {
                lang = LANG_COSMO_C;
            } else if (strcmp(arg, "amd64") == 0) {
                lang = LANG_AMD64;
            } else if (strcmp(arg, "aarch64") == 0) {
                lang = LANG_AARCH64;
            } else if (strcmp(arg, "masm64") == 0) {
                lang = LANG_MASM64;
            } else if (strcmp(arg, "masm32") == 0) {
                lang = LANG_MASM32;
            }
            editor_set_language(ed, lang);
            printf("Language: %s\n", syntax_language_name(lang));
        } else {
            printf("Usage: lang <cosmo|amd64|aarch64|masm64|masm32>\n");
        }
    }
    else if (strcmp(cmd, "menu") == 0) {
        if (arg[0]) {
            if (menu_load_ini(&g_app->menus, arg) == 0) {
                printf("Loaded menu: %s (%zu menus)\n", arg, g_app->menus.menu_count);
            } else {
                printf("Failed to load menu: %s\n", arg);
            }
        }
    }
    else if (strcmp(cmd, "template") == 0 || strcmp(cmd, "tpl") == 0) {
        if (arg[0] && ed) {
            char path[512];
            /* If no path separator, look in textape/ */
            if (!strchr(arg, '/') && !strchr(arg, '\\')) {
                snprintf(path, sizeof(path), "textape/%s", arg);
            } else {
                strncpy(path, arg, sizeof(path) - 1);
            }
            
            size_t len;
            char *content = file_read_all(path, &len);
            if (content) {
                size_t pos = editor_get_length(ed);
                editor_insert(ed, pos, content, len);
                printf("Inserted template: %s (%zu bytes)\n", path, len);
                free(content);
            } else {
                printf("Template not found: %s\n", path);
            }
        } else {
            printf("Usage: template <filename>\n");
            printf("  Looks in textape/ directory by default\n");
        }
    }
    else if (strcmp(cmd, "undo") == 0 || strcmp(cmd, "u") == 0) {
        if (ed) {
            editor_undo(ed);
            printf("Undo.\n");
        }
    }
    else if (strcmp(cmd, "redo") == 0) {
        if (ed) {
            editor_redo(ed);
            printf("Redo.\n");
        }
    }
    else if (strcmp(cmd, "history") == 0) {
        if (ed) {
            if (strcmp(arg, "export") == 0) {
                printf("Enter output file path: ");
                char out[256];
                if (fgets(out, sizeof(out), stdin)) {
                    str_trim(out);
                    if (editor_history_export(ed, out) == 0) {
                        printf("History exported to: %s\n", out);
                    } else {
                        printf("Failed to export history.\n");
                    }
                }
            } else if (strcmp(arg, "clear") == 0) {
                if (editor_history_clear(ed) == 0) {
                    printf("History cleared.\n");
                } else {
                    printf("Failed to clear history.\n");
                }
            } else {
                /* Show history info */
                printf("History:\n");
                printf("  File: %s\n", ed->file_path);
                printf("  Size: %zu bytes\n", editor_history_size(ed));
                printf("  Enabled: %s\n", ed->history_enabled ? "yes" : "no");
                if (ed->history) {
                    printf("  Has history: yes\n");
                } else {
                    printf("  Has history: no (save file first)\n");
                }
            }
        } else {
            printf("No active editor.\n");
        }
    }
    else if (cmd[0]) {
        printf("Unknown command: %s (type 'help' for commands)\n", cmd);
    }
}

int platform_init(AppState *app) {
    g_app = app;
    printf("tedit-cosmo CLI\n");
    printf("Type 'help' for commands.\n\n");
    return 0;
}

int platform_run(AppState *app) {
    char line[1024];
    
    while (app->running) {
        print_status();
        printf("> ");
        fflush(stdout);
        
        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }
        
        str_trim(line);
        handle_command(line);
    }
    
    return 0;
}

void platform_shutdown(AppState *app) {
    (void)app;
    printf("Goodbye.\n");
}

/* Stub implementations for platform functions */
int platform_open_file_dialog(char *path, size_t max, const char *filter) {
    (void)filter;
    printf("Enter file path: ");
    if (fgets(path, max, stdin)) {
        str_trim(path);
        return 0;
    }
    return -1;
}

int platform_save_file_dialog(char *path, size_t max, const char *filter) {
    return platform_open_file_dialog(path, max, filter);
}

int platform_folder_dialog(char *path, size_t max, const char *title) {
    (void)title;
    printf("Enter folder path: ");
    if (fgets(path, max, stdin)) {
        str_trim(path);
        return 0;
    }
    return -1;
}

int platform_message_box(const char *title, const char *msg, int type) {
    (void)type;
    printf("[%s] %s\n", title, msg);
    return 0;
}

int platform_clipboard_set(const char *text) {
    (void)text;
    return -1; /* Not supported in CLI */
}

char *platform_clipboard_get(void) {
    return NULL;
}

int platform_open_url(const char *url) {
    printf("URL: %s\n", url);
    return 0;
}

int platform_run_external(const char *cmd) {
    return system(cmd);
}

