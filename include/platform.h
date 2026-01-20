/*
 * platform.h - Platform abstraction layer
 */
#ifndef TEDIT_PLATFORM_H
#define TEDIT_PLATFORM_H

#include "app.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Platform-independent interface */
int platform_init(AppState *app);
int platform_run(AppState *app);
void platform_shutdown(AppState *app);

/* File dialogs */
int platform_open_file_dialog(char *path, size_t max, const char *filter);
int platform_save_file_dialog(char *path, size_t max, const char *filter);
int platform_folder_dialog(char *path, size_t max, const char *title);

/* Message boxes */
int platform_message_box(const char *title, const char *msg, int type);

/* Clipboard */
int platform_clipboard_set(const char *text);
char *platform_clipboard_get(void);

/* Shell */
int platform_open_url(const char *url);
int platform_run_external(const char *cmd);

#ifdef __cplusplus
}
#endif

#endif /* TEDIT_PLATFORM_H */

