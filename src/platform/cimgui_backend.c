/*
 * cimgui_backend.c - cimgui (Dear ImGui) GUI backend
 *
 * Uses cimgui (C bindings for Dear ImGui) with GLFW + OpenGL
 * for cross-platform GUI support.
 */

#ifdef PLATFORM_GUI

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

/* cimgui headers */
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "cimgui_impl.h"

/* GLFW */
#include <GLFW/glfw3.h>

static AppState *g_app = NULL;
static GLFWwindow *g_window = NULL;
static char g_text_buffer[1024 * 1024]; /* 1MB text buffer for editor */
static int g_show_about = 0;
static int g_show_find = 0;
static char g_find_text[256] = {0};
static char g_replace_text[256] = {0};

/* Colors for syntax highlighting */
static ImU32 color_default;
static ImU32 color_keyword;
static ImU32 color_register;
static ImU32 color_directive;
static ImU32 color_number;
static ImU32 color_string;
static ImU32 color_comment;

static void setup_colors(void) {
    color_default   = IM_COL32(220, 220, 220, 255);
    color_keyword   = IM_COL32(86, 156, 214, 255);   /* Blue */
    color_register  = IM_COL32(78, 201, 176, 255);   /* Teal */
    color_directive = IM_COL32(197, 134, 192, 255);  /* Purple */
    color_number    = IM_COL32(181, 206, 168, 255);  /* Light green */
    color_string    = IM_COL32(206, 145, 120, 255);  /* Orange */
    color_comment   = IM_COL32(106, 153, 85, 255);   /* Green */
}

static void sync_buffer_to_imgui(void) {
    EditorState *ed = app_get_active_editor(g_app);
    if (!ed) {
        g_text_buffer[0] = '\0';
        return;
    }
    
    size_t len = editor_get_length(ed);
    if (len >= sizeof(g_text_buffer)) {
        len = sizeof(g_text_buffer) - 1;
    }
    editor_get_text(ed, g_text_buffer, len + 1);
}

static void sync_imgui_to_buffer(void) {
    EditorState *ed = app_get_active_editor(g_app);
    if (!ed) return;
    
    editor_set_text(ed, g_text_buffer, strlen(g_text_buffer));
}

static void do_build(void) {
    EditorState *ed = app_get_active_editor(g_app);
    if (!ed || !ed->file_path[0]) return;
    
    sync_imgui_to_buffer();
    app_save_file(g_app, ed->file_path);
    
    char cmd[1024];
    menu_substitute_vars(cmd, sizeof(cmd), g_app->build.build_cmd,
                         ed->file_path, g_app->exe_dir);
    build_run_command(cmd);
}

static void do_run(void) {
    EditorState *ed = app_get_active_editor(g_app);
    if (!ed || !ed->file_path[0]) return;
    
    char cmd[1024];
    menu_substitute_vars(cmd, sizeof(cmd), g_app->build.run_cmd,
                         ed->file_path, g_app->exe_dir);
    build_run_command(cmd);
}

static void render_menu_bar(void) {
    if (igBeginMainMenuBar()) {
        /* File Menu */
        if (igBeginMenu("File", true)) {
            if (igMenuItem_Bool("New", "Ctrl+N", false, true)) {
                app_new_editor(g_app);
                sync_buffer_to_imgui();
            }
            if (igMenuItem_Bool("Open...", "Ctrl+O", false, true)) {
                char path[260];
                if (platform_open_file_dialog(path, sizeof(path), "*.c;*.h;*.asm") == 0) {
                    app_open_file(g_app, path);
                    sync_buffer_to_imgui();
                }
            }
            if (igMenuItem_Bool("Save", "Ctrl+S", false, true)) {
                EditorState *ed = app_get_active_editor(g_app);
                if (ed && ed->file_path[0]) {
                    sync_imgui_to_buffer();
                    app_save_file(g_app, ed->file_path);
                }
            }
            if (igMenuItem_Bool("Save As...", NULL, false, true)) {
                char path[260];
                if (platform_save_file_dialog(path, sizeof(path), "*.c;*.h;*.asm") == 0) {
                    sync_imgui_to_buffer();
                    app_save_file(g_app, path);
                    sync_buffer_to_imgui();
                }
            }
            igSeparator();
            if (igMenuItem_Bool("Exit", "Alt+F4", false, true)) {
                g_app->running = 0;
            }
            igEndMenu();
        }
        
        /* Edit Menu */
        if (igBeginMenu("Edit", true)) {
            if (igMenuItem_Bool("Undo", "Ctrl+Z", false, true)) {
                /* TODO: Undo */
            }
            if (igMenuItem_Bool("Redo", "Ctrl+Y", false, true)) {
                /* TODO: Redo */
            }
            igSeparator();
            if (igMenuItem_Bool("Cut", "Ctrl+X", false, true)) {
                /* Handled by ImGui */
            }
            if (igMenuItem_Bool("Copy", "Ctrl+C", false, true)) {
                /* Handled by ImGui */
            }
            if (igMenuItem_Bool("Paste", "Ctrl+V", false, true)) {
                /* Handled by ImGui */
            }
            igSeparator();
            if (igMenuItem_Bool("Select All", "Ctrl+A", false, true)) {
                /* TODO: Select all in editor */
            }
            igSeparator();
            if (igMenuItem_Bool("Find...", "Ctrl+F", false, true)) {
                g_show_find = 1;
            }
            igEndMenu();
        }
        
        /* Build Menu - like tEditor's Project/Assemble */
        if (igBeginMenu("Build", true)) {
            if (igMenuItem_Bool("Build", "F7", false, true)) {
                do_build();
            }
            if (igMenuItem_Bool("Run", "F5", false, true)) {
                do_run();
            }
            if (igMenuItem_Bool("Build && Run", "Ctrl+F5", false, true)) {
                do_build();
                do_run();
            }
            igEndMenu();
        }
        
        /* Help Menu */
        if (igBeginMenu("Help", true)) {
            if (igMenuItem_Bool("About", NULL, false, true)) {
                g_show_about = 1;
            }
            igEndMenu();
        }
        
        igEndMainMenuBar();
    }
}

/* Count lines in text buffer */
static int count_lines(const char *text) {
    int lines = 1;
    for (const char *p = text; *p; p++) {
        if (*p == '\n') lines++;
    }
    return lines;
}

/* Gutter width based on line count */
static float get_gutter_width(int line_count) {
    if (line_count < 100) return 40.0f;
    if (line_count < 1000) return 50.0f;
    if (line_count < 10000) return 60.0f;
    return 70.0f;
}

/* Track scroll position for syncing */
static float g_editor_scroll_y = 0.0f;

static void render_editor(void) {
    ImGuiIO *io = igGetIO();
    
    /* Main editor window fills available space */
    ImVec2 size;
    size.x = io->DisplaySize.x;
    size.y = io->DisplaySize.y - 50; /* Leave room for menu and status */
    
    igSetNextWindowPos((ImVec2){0, 20}, ImGuiCond_Always, (ImVec2){0, 0});
    igSetNextWindowSize(size, ImGuiCond_Always);
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | 
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoScrollbar;
    
    if (igBegin("Editor", NULL, flags)) {
        int line_count = count_lines(g_text_buffer);
        float gutter_width = get_gutter_width(line_count);
        float line_height = igGetTextLineHeight();
        
        ImVec2 content_size;
        igGetContentRegionAvail(&content_size);
        
        /* === LINE NUMBER GUTTER === */
        ImVec2 gutter_size = {gutter_width, content_size.y};
        
        igBeginChild_Str("##gutter", gutter_size, false, 
                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        {
            /* Sync scroll with editor */
            igSetScrollY_Float(g_editor_scroll_y);
            
            /* Draw line numbers */
            ImDrawList *draw_list = igGetWindowDrawList();
            ImVec2 cursor_pos;
            igGetCursorScreenPos(&cursor_pos);
            
            ImU32 line_num_color = IM_COL32(140, 140, 140, 255);  /* Gray */
            ImU32 gutter_bg = IM_COL32(30, 30, 30, 255);          /* Dark background */
            
            /* Gutter background */
            ImVec2 gutter_min = cursor_pos;
            ImVec2 gutter_max = {cursor_pos.x + gutter_width - 5, cursor_pos.y + line_count * line_height};
            ImDrawList_AddRectFilled(draw_list, gutter_min, gutter_max, gutter_bg, 0.0f, 0);
            
            /* Draw each line number */
            for (int i = 1; i <= line_count; i++) {
                char line_str[16];
                snprintf(line_str, sizeof(line_str), "%d", i);
                
                /* Right-align line numbers */
                ImVec2 text_size;
                igCalcTextSize(&text_size, line_str, NULL, false, 0);
                
                ImVec2 text_pos;
                text_pos.x = cursor_pos.x + gutter_width - text_size.x - 10;
                text_pos.y = cursor_pos.y + (i - 1) * line_height;
                
                ImDrawList_AddText_Vec2(draw_list, text_pos, line_num_color, line_str, NULL);
            }
            
            /* Reserve space for all lines */
            igDummy((ImVec2){gutter_width, line_count * line_height});
        }
        igEndChild();
        
        /* Same line - editor next to gutter */
        igSameLine(0, 0);
        
        /* === TEXT EDITOR === */
        ImVec2 editor_size;
        editor_size.x = content_size.x - gutter_width;
        editor_size.y = content_size.y;
        
        ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_AllowTabInput;
        
        /* Use a child window to capture scroll */
        igBeginChild_Str("##editor_child", editor_size, false, 0);
        {
            ImVec2 input_size = {-1, -1};
            
            if (igInputTextMultiline("##editor", g_text_buffer, sizeof(g_text_buffer),
                                      input_size, input_flags, NULL, NULL)) {
                EditorState *ed = app_get_active_editor(g_app);
                if (ed) ed->dirty = 1;
            }
            
            /* Capture scroll position for gutter sync */
            g_editor_scroll_y = igGetScrollY();
        }
        igEndChild();
    }
    igEnd();
}

static void render_status_bar(void) {
    ImGuiIO *io = igGetIO();
    
    ImVec2 pos;
    pos.x = 0;
    pos.y = io->DisplaySize.y - 25;
    
    ImVec2 size;
    size.x = io->DisplaySize.x;
    size.y = 25;
    
    igSetNextWindowPos(pos, ImGuiCond_Always, (ImVec2){0, 0});
    igSetNextWindowSize(size, ImGuiCond_Always);
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoScrollbar;
    
    if (igBegin("Status", NULL, flags)) {
        EditorState *ed = app_get_active_editor(g_app);
        if (ed) {
            size_t line, col;
            editor_get_cursor_pos(ed, &line, &col);
            
            igText("%s%s | %s | Ln %zu, Col %zu",
                   ed->file_path[0] ? ed->file_path : "Untitled",
                   ed->dirty ? " *" : "",
                   syntax_language_name(ed->language),
                   line, col);
        } else {
            igText("Ready");
        }
    }
    igEnd();
}

static void render_about_dialog(void) {
    if (!g_show_about) return;
    
    igSetNextWindowSize((ImVec2){400, 200}, ImGuiCond_FirstUseEver);
    if (igBegin("About tedit-cosmo", &g_show_about, 0)) {
        igText("tedit-cosmo");
        igText("A FOSS clone of tEditor (MASM64 SDK)");
        igText("");
        igText("Built with:");
        igBulletText("Cosmopolitan C");
        igBulletText("cimgui (Dear ImGui)");
        igBulletText("GLFW + OpenGL");
        igText("");
        if (igButton("OK", (ImVec2){100, 0})) {
            g_show_about = 0;
        }
    }
    igEnd();
}

static void render_find_dialog(void) {
    if (!g_show_find) return;
    
    igSetNextWindowSize((ImVec2){400, 150}, ImGuiCond_FirstUseEver);
    if (igBegin("Find/Replace", &g_show_find, 0)) {
        igText("Find:");
        igInputText("##find", g_find_text, sizeof(g_find_text), 0, NULL, NULL);
        
        igText("Replace:");
        igInputText("##replace", g_replace_text, sizeof(g_replace_text), 0, NULL, NULL);
        
        if (igButton("Find Next", (ImVec2){100, 0})) {
            /* TODO: Find implementation */
        }
        igSameLine(0, 10);
        if (igButton("Replace", (ImVec2){100, 0})) {
            /* TODO: Replace implementation */
        }
        igSameLine(0, 10);
        if (igButton("Replace All", (ImVec2){100, 0})) {
            /* TODO: Replace all implementation */
        }
    }
    igEnd();
}

static void glfw_error_callback(int error, const char *description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int platform_init(AppState *app) {
    g_app = app;
    app->gui_mode = 1;
    
    glfwSetErrorCallback(glfw_error_callback);
    
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }
    
    /* OpenGL 3.3 Core */
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    
    g_window = glfwCreateWindow(
        app->config.window_w,
        app->config.window_h,
        "tedit-cosmo",
        NULL, NULL
    );
    
    if (!g_window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(g_window);
    glfwSwapInterval(1); /* VSync */
    
    /* Initialize ImGui */
    igCreateContext(NULL);
    ImGuiIO *io = igGetIO();
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    /* Setup ImGui style */
    igStyleColorsDark(NULL);
    
    /* Initialize backends */
    ImGui_ImplGlfw_InitForOpenGL(g_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    
    setup_colors();
    sync_buffer_to_imgui();
    
    return 0;
}

int platform_run(AppState *app) {
    while (!glfwWindowShouldClose(g_window) && app->running) {
        glfwPollEvents();
        
        /* Start ImGui frame */
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        igNewFrame();
        
        /* Render UI */
        render_menu_bar();
        render_editor();
        render_status_bar();
        render_about_dialog();
        render_find_dialog();
        
        /* Render */
        igRender();
        
        int display_w, display_h;
        glfwGetFramebufferSize(g_window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        ImGui_ImplOpenGL3_RenderDrawData(igGetDrawData());
        
        glfwSwapBuffers(g_window);
    }
    
    /* Save buffer before exit */
    sync_imgui_to_buffer();
    
    return 0;
}

void platform_shutdown(AppState *app) {
    (void)app;
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    igDestroyContext(NULL);
    
    glfwDestroyWindow(g_window);
    glfwTerminate();
}

/* Platform dialogs - use tinyfiledialogs or fallback */
int platform_open_file_dialog(char *path, size_t max, const char *filter) {
    (void)filter;
    /* TODO: Use tinyfiledialogs */
    printf("Open file: ");
    if (fgets(path, max, stdin)) {
        str_trim(path);
        return path[0] ? 0 : -1;
    }
    return -1;
}

int platform_save_file_dialog(char *path, size_t max, const char *filter) {
    return platform_open_file_dialog(path, max, filter);
}

int platform_folder_dialog(char *path, size_t max, const char *title) {
    (void)title;
    printf("Folder: ");
    if (fgets(path, max, stdin)) {
        str_trim(path);
        return path[0] ? 0 : -1;
    }
    return -1;
}

int platform_message_box(const char *title, const char *msg, int type) {
    (void)type;
    printf("[%s] %s\n", title, msg);
    return 0;
}

int platform_clipboard_set(const char *text) {
    if (g_window) {
        glfwSetClipboardString(g_window, text);
        return 0;
    }
    return -1;
}

char *platform_clipboard_get(void) {
    if (g_window) {
        const char *text = glfwGetClipboardString(g_window);
        return text ? str_dup(text) : NULL;
    }
    return NULL;
}

int platform_open_url(const char *url) {
    char cmd[512];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "start \"\" \"%s\"", url);
#elif __APPLE__
    snprintf(cmd, sizeof(cmd), "open \"%s\"", url);
#else
    snprintf(cmd, sizeof(cmd), "xdg-open \"%s\"", url);
#endif
    return system(cmd);
}

int platform_run_external(const char *cmd) {
    return system(cmd);
}

#endif /* PLATFORM_GUI */

