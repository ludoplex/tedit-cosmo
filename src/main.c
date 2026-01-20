/*
 * main.c - tedit-cosmo entry point
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "platform.h"
#include "editor.h"
#include "history.h"
#include "backup.h"

static void print_usage(void) {
    printf("tedit-cosmo - Portable code editor\n\n");
    printf("Usage: tedit [options] [file]\n\n");
    printf("Options:\n");
    printf("  --help                      Show this help\n");
    printf("  --version                   Show version\n");
    printf("  --history-export <file> <out>   Export history to text file\n");
    printf("  --history-compact <file>    Compact and archive history\n");
    printf("  --history-clear <file>      Clear all history for file\n");
    printf("  --history-info <file>       Show history info for file\n");
    printf("  --backup <destination>      Create backup to destination\n");
    printf("\n");
}

static void print_version(void) {
    printf("tedit-cosmo 0.1.0\n");
    printf("Built with Cosmopolitan C\n");
}

/* Handle --history-export <file> <output> */
static int cmd_history_export(const char *file, const char *output) {
    History *h = history_open(file);
    if (!h) {
        fprintf(stderr, "Failed to open history for: %s\n", file);
        return 1;
    }
    
    if (history_export(h, output) != 0) {
        fprintf(stderr, "Failed to export history to: %s\n", output);
        history_close(h);
        return 1;
    }
    
    printf("Exported %zu operations to: %s\n", history_count(h), output);
    history_close(h);
    return 0;
}

/* Handle --history-compact <file> */
static int cmd_history_compact(const char *file) {
    History *h = history_open(file);
    if (!h) {
        fprintf(stderr, "Failed to open history for: %s\n", file);
        return 1;
    }
    
    /* Archive to <file>.history-archive */
    char archive[512];
    snprintf(archive, sizeof(archive), "%s.history-archive-%lu", 
             file, (unsigned long)history_get_timestamp());
    
    size_t old_count = history_count(h);
    if (history_compact(h, archive) != 0) {
        fprintf(stderr, "Failed to compact history\n");
        history_close(h);
        return 1;
    }
    
    printf("Compacted history: %zu ops archived to %s\n", old_count, archive);
    history_close(h);
    return 0;
}

/* Handle --history-clear <file> */
static int cmd_history_clear(const char *file) {
    History *h = history_open(file);
    if (!h) {
        fprintf(stderr, "Failed to open history for: %s\n", file);
        return 1;
    }
    
    size_t old_count = history_count(h);
    if (history_clear(h) != 0) {
        fprintf(stderr, "Failed to clear history\n");
        history_close(h);
        return 1;
    }
    
    printf("Cleared %zu operations from history\n", old_count);
    history_close(h);
    return 0;
}

/* Handle --history-info <file> */
static int cmd_history_info(const char *file) {
    History *h = history_open(file);
    if (!h) {
        fprintf(stderr, "Failed to open history for: %s\n", file);
        return 1;
    }
    
    char hist_path[512];
    history_get_path(file, hist_path, sizeof(hist_path));
    
    printf("History for: %s\n", file);
    printf("  History file: %s\n", hist_path);
    printf("  Operations: %zu\n", history_count(h));
    printf("  File size: %zu bytes\n", history_size(h));
    printf("  Can undo: %s\n", history_can_undo(h) ? "yes" : "no");
    printf("  Can redo: %s\n", history_can_redo(h) ? "yes" : "no");
    
    history_close(h);
    return 0;
}

/* Handle --backup <destination> [project_dir] */
static int cmd_backup(const char *dest, const char *project_dir) {
    BackupConfig cfg;
    char ini_path[512];
    
    /* Try to find backup.ini */
    backup_get_default_ini_path(ini_path, sizeof(ini_path), ".");
    
    if (backup_config_load(&cfg, ini_path) != 0) {
        fprintf(stderr, "Failed to load backup.ini from: %s\n", ini_path);
        fprintf(stderr, "Create backup.ini with destination definitions.\n");
        return 1;
    }
    
    BackupDest *d = backup_config_get_dest(&cfg, dest);
    if (!d) {
        fprintf(stderr, "Unknown destination: %s\n", dest);
        fprintf(stderr, "Available destinations:\n");
        for (size_t i = 0; i < cfg.dest_count; i++) {
            fprintf(stderr, "  %s\n", cfg.destinations[i].name);
        }
        backup_config_free(&cfg);
        return 1;
    }
    
    const char *dir = project_dir ? project_dir : ".";
    printf("Creating backup to '%s'...\n", dest);
    
    int result = backup_project(&cfg, dest, dir, 1);
    
    if (result == 0) {
        printf("Backup completed successfully.\n");
    } else {
        fprintf(stderr, "Backup failed.\n");
    }
    
    backup_config_free(&cfg);
    return result == 0 ? 0 : 1;
}

/* Handle --backup-list */
static int cmd_backup_list(void) {
    BackupConfig cfg;
    char ini_path[512];
    
    backup_get_default_ini_path(ini_path, sizeof(ini_path), ".");
    
    if (backup_config_load(&cfg, ini_path) != 0) {
        fprintf(stderr, "Failed to load backup.ini\n");
        return 1;
    }
    
    printf("Backup destinations:\n");
    for (size_t i = 0; i < cfg.dest_count; i++) {
        printf("  %s: %s\n", cfg.destinations[i].name, cfg.destinations[i].command);
    }
    
    backup_config_free(&cfg);
    return 0;
}

int main(int argc, char **argv) {
    /* Handle command-line flags */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        }
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            print_version();
            return 0;
        }
        if (strcmp(argv[i], "--history-export") == 0) {
            if (i + 2 >= argc) {
                fprintf(stderr, "Usage: --history-export <file> <output>\n");
                return 1;
            }
            return cmd_history_export(argv[i+1], argv[i+2]);
        }
        if (strcmp(argv[i], "--history-compact") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Usage: --history-compact <file>\n");
                return 1;
            }
            return cmd_history_compact(argv[i+1]);
        }
        if (strcmp(argv[i], "--history-clear") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Usage: --history-clear <file>\n");
                return 1;
            }
            return cmd_history_clear(argv[i+1]);
        }
        if (strcmp(argv[i], "--history-info") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Usage: --history-info <file>\n");
                return 1;
            }
            return cmd_history_info(argv[i+1]);
        }
        if (strcmp(argv[i], "--backup") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Usage: --backup <destination> [project_dir]\n");
                return 1;
            }
            const char *dest = argv[i+1];
            const char *dir = (i + 2 < argc && argv[i+2][0] != '-') ? argv[i+2] : NULL;
            return cmd_backup(dest, dir);
        }
        if (strcmp(argv[i], "--backup-list") == 0) {
            return cmd_backup_list();
        }
    }
    
    /* Normal editor startup */
    AppState app = {0};
    
    if (app_init(&app) != 0) {
        fprintf(stderr, "Failed to initialize application\n");
        return 1;
    }
    
    /* Open file from command line (skip flags) */
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            app_open_file(&app, argv[i]);
            break;
        }
    }
    
    if (platform_init(&app) != 0) {
        fprintf(stderr, "Failed to initialize platform\n");
        app_shutdown(&app);
        return 1;
    }
    
    int result = platform_run(&app);
    
    platform_shutdown(&app);
    app_shutdown(&app);
    
    return result;
}
