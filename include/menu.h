/*
 * menu.h - INI-based menu system
 */
#ifndef TEDIT_MENU_H
#define TEDIT_MENU_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_MENU_ITEMS 64
#define MAX_MENUS 16

typedef struct MenuItem {
    char label[128];
    char command[512];
    char accelerator[32];
    int is_separator;
    int id;
} MenuItem;

typedef struct Menu {
    char name[64];
    MenuItem items[MAX_MENU_ITEMS];
    size_t item_count;
} Menu;

typedef struct MenuSet {
    Menu menus[MAX_MENUS];
    size_t menu_count;
} MenuSet;

int menu_load_ini(MenuSet *set, const char *path);
void menu_free(MenuSet *set);

const MenuItem *menu_find_by_id(MenuSet *set, int id);
void menu_substitute_vars(char *out, size_t max, const char *cmd,
                          const char *filepath, const char *exe_dir);

#ifdef __cplusplus
}
#endif

#endif /* TEDIT_MENU_H */

