/*
 * config.c - Configuration management
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "util.h"

void config_defaults(Config *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    strcpy(cfg->font_name, "Consolas");
    cfg->font_size = 12;
    cfg->tab_width = 4;
    cfg->use_spaces = 0;
    cfg->show_line_numbers = 1;
    cfg->word_wrap = 0;
    cfg->window_x = 100;
    cfg->window_y = 100;
    cfg->window_w = 900;
    cfg->window_h = 700;
}

int config_load(Config *cfg, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        str_trim(line);
        if (line[0] == ';' || line[0] == '#' || line[0] == '\0') continue;
        
        char *eq = strchr(line, '=');
        if (!eq) continue;
        
        *eq = '\0';
        char *key = str_trim(line);
        char *val = str_trim(eq + 1);
        
        if (strcmp(key, "font_name") == 0) {
            strncpy(cfg->font_name, val, sizeof(cfg->font_name) - 1);
        } else if (strcmp(key, "font_size") == 0) {
            cfg->font_size = atoi(val);
        } else if (strcmp(key, "tab_width") == 0) {
            cfg->tab_width = atoi(val);
        } else if (strcmp(key, "use_spaces") == 0) {
            cfg->use_spaces = atoi(val);
        } else if (strcmp(key, "show_line_numbers") == 0) {
            cfg->show_line_numbers = atoi(val);
        } else if (strcmp(key, "word_wrap") == 0) {
            cfg->word_wrap = atoi(val);
        }
    }
    
    fclose(f);
    return 0;
}

int config_save(Config *cfg, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    
    fprintf(f, "; tedit-cosmo configuration\n");
    fprintf(f, "font_name=%s\n", cfg->font_name);
    fprintf(f, "font_size=%d\n", cfg->font_size);
    fprintf(f, "tab_width=%d\n", cfg->tab_width);
    fprintf(f, "use_spaces=%d\n", cfg->use_spaces);
    fprintf(f, "show_line_numbers=%d\n", cfg->show_line_numbers);
    fprintf(f, "word_wrap=%d\n", cfg->word_wrap);
    
    /* Recent files */
    fprintf(f, "\n; Recent files\n");
    for (size_t i = 0; i < cfg->recent_count; i++) {
        fprintf(f, "recent%zu=%s\n", i, cfg->recent_files[i]);
    }
    
    fclose(f);
    return 0;
}

void config_add_recent(Config *cfg, const char *path) {
    /* Check if already in list */
    for (size_t i = 0; i < cfg->recent_count; i++) {
        if (strcmp(cfg->recent_files[i], path) == 0) {
            /* Move to front */
            char tmp[260];
            strcpy(tmp, cfg->recent_files[i]);
            for (size_t j = i; j > 0; j--) {
                strcpy(cfg->recent_files[j], cfg->recent_files[j-1]);
            }
            strcpy(cfg->recent_files[0], tmp);
            return;
        }
    }
    
    /* Shift existing entries */
    if (cfg->recent_count < MAX_RECENT_FILES) {
        cfg->recent_count++;
    }
    for (size_t i = cfg->recent_count - 1; i > 0; i--) {
        strcpy(cfg->recent_files[i], cfg->recent_files[i-1]);
    }
    
    strncpy(cfg->recent_files[0], path, sizeof(cfg->recent_files[0]) - 1);
}

