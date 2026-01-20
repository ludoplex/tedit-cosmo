/*
 * menu.c - INI-based menu system
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "menu.h"
#include "util.h"

static int next_menu_id = 2000;

int menu_load_ini(MenuSet *set, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    
    memset(set, 0, sizeof(*set));
    
    char line[512];
    Menu *current_menu = NULL;
    
    while (fgets(line, sizeof(line), f)) {
        str_trim(line);
        
        /* Skip empty lines and comments */
        if (line[0] == '\0' || line[0] == ';') continue;
        
        /* Section header [MenuName] */
        if (line[0] == '[') {
            char *end = strchr(line, ']');
            if (end && set->menu_count < MAX_MENUS) {
                *end = '\0';
                current_menu = &set->menus[set->menu_count++];
                strncpy(current_menu->name, line + 1, sizeof(current_menu->name) - 1);
            }
            continue;
        }
        
        if (!current_menu) continue;
        if (current_menu->item_count >= MAX_MENU_ITEMS) continue;
        
        /* Separator */
        if (strcmp(line, "-") == 0) {
            MenuItem *item = &current_menu->items[current_menu->item_count++];
            item->is_separator = 1;
            item->id = 0;
            continue;
        }
        
        /* Menu item: Label,Command */
        char *comma = strchr(line, ',');
        if (comma) {
            *comma = '\0';
            MenuItem *item = &current_menu->items[current_menu->item_count++];
            strncpy(item->label, line, sizeof(item->label) - 1);
            strncpy(item->command, comma + 1, sizeof(item->command) - 1);
            item->is_separator = 0;
            item->id = next_menu_id++;
            
            /* Extract accelerator from label */
            char *tab = strchr(item->label, '\t');
            if (tab) {
                strncpy(item->accelerator, tab + 1, sizeof(item->accelerator) - 1);
            }
        }
    }
    
    fclose(f);
    return 0;
}

void menu_free(MenuSet *set) {
    memset(set, 0, sizeof(*set));
}

const MenuItem *menu_find_by_id(MenuSet *set, int id) {
    for (size_t m = 0; m < set->menu_count; m++) {
        for (size_t i = 0; i < set->menus[m].item_count; i++) {
            if (set->menus[m].items[i].id == id) {
                return &set->menus[m].items[i];
            }
        }
    }
    return NULL;
}

void menu_substitute_vars(char *out, size_t max, const char *cmd,
                          const char *filepath, const char *exe_dir) {
    char base[260] = {0};
    char filename[260] = {0};
    const char *dot;
    
    if (filepath && filepath[0]) {
        /* Get basename without extension */
        const char *fn = path_basename(filepath);
        strncpy(filename, fn, sizeof(filename) - 1);
        strncpy(base, filepath, sizeof(base) - 1);
        dot = strrchr(base, '.');
        if (dot) {
            size_t pos = dot - base;
            base[pos] = '\0';
        }
    }
    
    size_t i = 0, o = 0;
    while (cmd[i] && o + 1 < max) {
        if (cmd[i] == '{') {
            /* {b} = base name (path without extension) */
            if (strncmp(cmd + i, "{b}", 3) == 0) {
                o += snprintf(out + o, max - o, "%s", base);
                i += 3;
                continue;
            }
            /* {n} = filename only */
            if (strncmp(cmd + i, "{n}", 3) == 0) {
                o += snprintf(out + o, max - o, "%s", filename);
                i += 3;
                continue;
            }
            /* {e} = executable directory */
            if (strncmp(cmd + i, "{e}", 3) == 0) {
                o += snprintf(out + o, max - o, "%s", exe_dir ? exe_dir : "");
                i += 3;
                continue;
            }
            /* {in} = full input path */
            if (strncmp(cmd + i, "{in}", 4) == 0) {
                o += snprintf(out + o, max - o, "%s", filepath ? filepath : "");
                i += 4;
                continue;
            }
            /* {out} = output path (.com) */
            if (strncmp(cmd + i, "{out}", 5) == 0) {
                o += snprintf(out + o, max - o, "%s.com", base);
                i += 5;
                continue;
            }
        }
        out[o++] = cmd[i++];
    }
    out[o] = '\0';
}

