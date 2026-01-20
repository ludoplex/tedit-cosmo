/*
 * config.h - Application configuration
 */
#ifndef TEDIT_CONFIG_H
#define TEDIT_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_RECENT_FILES 16

typedef struct Config {
    char font_name[64];
    int font_size;
    int tab_width;
    int use_spaces;
    int show_line_numbers;
    int word_wrap;
    char recent_files[MAX_RECENT_FILES][260];
    size_t recent_count;
    int window_x, window_y;
    int window_w, window_h;
} Config;

void config_defaults(Config *cfg);
int config_load(Config *cfg, const char *path);
int config_save(Config *cfg, const char *path);

void config_add_recent(Config *cfg, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* TEDIT_CONFIG_H */

