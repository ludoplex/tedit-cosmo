/*
 * backup.h - Extensible backup system
 * 
 * Provides basic archiving functionality and command execution for
 * backup destinations defined in backup.ini
 */
#ifndef TEDIT_BACKUP_H
#define TEDIT_BACKUP_H

#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BACKUP_MAX_DESTINATIONS 16
#define BACKUP_MAX_NAME 64
#define BACKUP_MAX_CMD 512
#define BACKUP_MAX_PATH 260

/* Backup destination (from backup.ini) */
typedef struct BackupDest {
    char name[BACKUP_MAX_NAME];     /* Destination name (e.g., "local", "s3") */
    char command[BACKUP_MAX_CMD];   /* Command template with variables */
} BackupDest;

/* Backup settings */
typedef struct BackupSettings {
    int threshold_mb;               /* Prompt when history exceeds this */
    int interval;                   /* Auto-backup interval in seconds (0 = disabled) */
    char archive_format[16];        /* "tar.gz" or "zip" */
    char temp_dir[BACKUP_MAX_PATH]; /* Temporary directory */
    char default_dest[BACKUP_MAX_NAME]; /* Default destination */
} BackupSettings;

/* Backup configuration */
typedef struct BackupConfig {
    BackupSettings settings;
    BackupDest destinations[BACKUP_MAX_DESTINATIONS];
    size_t dest_count;
    char ini_path[BACKUP_MAX_PATH]; /* Path to backup.ini */
} BackupConfig;

/* Archive entry */
typedef struct ArchiveEntry {
    char path[BACKUP_MAX_PATH];     /* Relative path in archive */
    char source[BACKUP_MAX_PATH];   /* Source file path */
} ArchiveEntry;

/* Archive builder */
typedef struct Archive {
    char path[BACKUP_MAX_PATH];     /* Archive output path */
    ArchiveEntry *entries;
    size_t entry_count;
    size_t entry_capacity;
} Archive;

/* Configuration */
int backup_config_load(BackupConfig *cfg, const char *ini_path);
void backup_config_free(BackupConfig *cfg);
BackupDest *backup_config_get_dest(BackupConfig *cfg, const char *name);

/* Archive creation */
Archive *archive_create(const char *output_path);
void archive_destroy(Archive *ar);
int archive_add_file(Archive *ar, const char *source, const char *dest_path);
int archive_add_directory(Archive *ar, const char *dir, const char *prefix);
int archive_finalize(Archive *ar);  /* Create the actual archive file */

/* Backup execution */
int backup_execute(BackupConfig *cfg, const char *dest_name,
                   const char *project_dir, const char *archive_path);

/* Variable substitution for backup commands */
typedef struct BackupVars {
    char archive[BACKUP_MAX_PATH];  /* {archive} - Generated archive path */
    char project[BACKUP_MAX_PATH];  /* {p} - Project directory */
    char file[BACKUP_MAX_PATH];     /* {e} - Current file */
    char name[BACKUP_MAX_NAME];     /* {n} - File name without extension */
    char bindir[BACKUP_MAX_PATH];   /* {b} - Binary directory */
    char date[16];                  /* {date} - YYYY-MM-DD */
    char time_str[16];              /* {time} - HHMMSS */
} BackupVars;

void backup_vars_init(BackupVars *vars, const char *archive, 
                      const char *project, const char *file,
                      const char *bindir);
int backup_substitute(char *out, size_t out_size, 
                      const char *template_cmd, BackupVars *vars);

/* High-level backup operations */
int backup_project(BackupConfig *cfg, const char *dest_name,
                   const char *project_dir, int include_history);

/* Get default backup.ini path */
void backup_get_default_ini_path(char *out, size_t out_size, const char *exe_dir);

#ifdef __cplusplus
}
#endif

#endif /* TEDIT_BACKUP_H */


