/*
 * backup.c - Extensible backup system implementation
 * 
 * Provides basic tar archive creation and backup destination management.
 * Compression uses a simple store method for portability.
 * For full gzip compression, use external tools via command templates.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define PATH_SEP '\\'
#else
#include <dirent.h>
#include <unistd.h>
#define PATH_SEP '/'
#endif

#include "backup.h"
#include "config.h"
#include "util.h"

/* TAR header structure (POSIX ustar format) */
#pragma pack(push, 1)
typedef struct TarHeader {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char padding[12];
} TarHeader;
#pragma pack(pop)

/* Calculate TAR checksum */
static unsigned int tar_checksum(TarHeader *header) {
    unsigned int sum = 0;
    unsigned char *p = (unsigned char *)header;
    
    /* Fill checksum field with spaces for calculation */
    memset(header->checksum, ' ', 8);
    
    for (int i = 0; i < 512; i++) {
        sum += p[i];
    }
    
    return sum;
}

/* Initialize a TAR header for a file */
static void tar_init_header(TarHeader *header, const char *name, 
                            size_t size, time_t mtime) {
    memset(header, 0, sizeof(TarHeader));
    
    /* Handle long names with prefix */
    size_t name_len = strlen(name);
    if (name_len <= 100) {
        strncpy(header->name, name, 100);
    } else if (name_len <= 255) {
        /* Split into prefix and name */
        const char *split = name + name_len - 100;
        while (split > name && *split != '/' && *split != '\\') {
            split--;
        }
        if (split > name) {
            size_t prefix_len = split - name;
            strncpy(header->prefix, name, prefix_len < 155 ? prefix_len : 155);
            strncpy(header->name, split + 1, 100);
        } else {
            strncpy(header->name, name, 100);
        }
    } else {
        strncpy(header->name, name + name_len - 100, 100);
    }
    
    snprintf(header->mode, 8, "%07o", 0644);
    snprintf(header->uid, 8, "%07o", 0);
    snprintf(header->gid, 8, "%07o", 0);
    snprintf(header->size, 12, "%011lo", (unsigned long)size);
    snprintf(header->mtime, 12, "%011lo", (unsigned long)mtime);
    header->typeflag = '0';  /* Regular file */
    memcpy(header->magic, "ustar", 5);
    header->magic[5] = '\0';
    memcpy(header->version, "00", 2);
    strcpy(header->uname, "tedit");
    strcpy(header->gname, "tedit");
    
    /* Calculate and set checksum */
    unsigned int cksum = tar_checksum(header);
    snprintf(header->checksum, 8, "%06o", cksum);
    header->checksum[6] = '\0';
    header->checksum[7] = ' ';
}

/* Create a new archive */
Archive *archive_create(const char *output_path) {
    Archive *ar = calloc(1, sizeof(Archive));
    if (!ar) return NULL;
    
    strncpy(ar->path, output_path, BACKUP_MAX_PATH - 1);
    ar->entry_capacity = 64;
    ar->entries = calloc(ar->entry_capacity, sizeof(ArchiveEntry));
    if (!ar->entries) {
        free(ar);
        return NULL;
    }
    
    return ar;
}

/* Destroy archive builder */
void archive_destroy(Archive *ar) {
    if (ar) {
        free(ar->entries);
        free(ar);
    }
}

/* Add a file to the archive */
int archive_add_file(Archive *ar, const char *source, const char *dest_path) {
    if (!ar || ar->entry_count >= ar->entry_capacity) {
        /* Grow capacity */
        if (ar) {
            size_t new_cap = ar->entry_capacity * 2;
            ArchiveEntry *new_entries = realloc(ar->entries, 
                                                 new_cap * sizeof(ArchiveEntry));
            if (!new_entries) return -1;
            ar->entries = new_entries;
            ar->entry_capacity = new_cap;
        } else {
            return -1;
        }
    }
    
    ArchiveEntry *entry = &ar->entries[ar->entry_count++];
    strncpy(entry->source, source, BACKUP_MAX_PATH - 1);
    strncpy(entry->path, dest_path, BACKUP_MAX_PATH - 1);
    
    return 0;
}

/* Recursively add a directory */
int archive_add_directory(Archive *ar, const char *dir, const char *prefix) {
#ifdef _WIN32
    char pattern[BACKUP_MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*", dir);
    
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return -1;
    
    do {
        if (fd.cFileName[0] == '.') continue;
        
        char source[BACKUP_MAX_PATH];
        char dest[BACKUP_MAX_PATH];
        snprintf(source, sizeof(source), "%s\\%s", dir, fd.cFileName);
        
        if (prefix && prefix[0]) {
            snprintf(dest, sizeof(dest), "%s/%s", prefix, fd.cFileName);
        } else {
            strncpy(dest, fd.cFileName, sizeof(dest) - 1);
        }
        
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            archive_add_directory(ar, source, dest);
        } else {
            archive_add_file(ar, source, dest);
        }
    } while (FindNextFileA(hFind, &fd));
    
    FindClose(hFind);
#else
    DIR *d = opendir(dir);
    if (!d) return -1;
    
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        
        char source[BACKUP_MAX_PATH];
        char dest[BACKUP_MAX_PATH];
        snprintf(source, sizeof(source), "%s/%s", dir, entry->d_name);
        
        if (prefix && prefix[0]) {
            snprintf(dest, sizeof(dest), "%s/%s", prefix, entry->d_name);
        } else {
            strncpy(dest, entry->d_name, sizeof(dest) - 1);
        }
        
        struct stat st;
        if (stat(source, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                archive_add_directory(ar, source, dest);
            } else {
                archive_add_file(ar, source, dest);
            }
        }
    }
    
    closedir(d);
#endif
    return 0;
}

/* Finalize and write the archive */
int archive_finalize(Archive *ar) {
    if (!ar) return -1;
    
    FILE *f = fopen(ar->path, "wb");
    if (!f) return -1;
    
    char buf[4096];
    
    for (size_t i = 0; i < ar->entry_count; i++) {
        ArchiveEntry *entry = &ar->entries[i];
        
        /* Get file info */
        FILE *src = fopen(entry->source, "rb");
        if (!src) continue;
        
        fseek(src, 0, SEEK_END);
        long size = ftell(src);
        fseek(src, 0, SEEK_SET);
        
        if (size < 0) {
            fclose(src);
            continue;
        }
        
        /* Write header */
        TarHeader header;
        tar_init_header(&header, entry->path, (size_t)size, time(NULL));
        fwrite(&header, 1, 512, f);
        
        /* Write file content */
        size_t remaining = (size_t)size;
        while (remaining > 0) {
            size_t to_read = remaining < sizeof(buf) ? remaining : sizeof(buf);
            size_t read = fread(buf, 1, to_read, src);
            if (read == 0) break;
            fwrite(buf, 1, read, f);
            remaining -= read;
        }
        
        fclose(src);
        
        /* Pad to 512-byte boundary */
        size_t padding = (512 - (size % 512)) % 512;
        if (padding > 0) {
            memset(buf, 0, padding);
            fwrite(buf, 1, padding, f);
        }
    }
    
    /* Write two empty blocks to end archive */
    memset(buf, 0, 1024);
    fwrite(buf, 1, 1024, f);
    
    fclose(f);
    return 0;
}

/* Initialize backup variables */
void backup_vars_init(BackupVars *vars, const char *archive, 
                      const char *project, const char *file,
                      const char *bindir) {
    memset(vars, 0, sizeof(BackupVars));
    
    if (archive) strncpy(vars->archive, archive, BACKUP_MAX_PATH - 1);
    if (project) strncpy(vars->project, project, BACKUP_MAX_PATH - 1);
    if (file) strncpy(vars->file, file, BACKUP_MAX_PATH - 1);
    if (bindir) strncpy(vars->bindir, bindir, BACKUP_MAX_PATH - 1);
    
    /* Extract name without extension */
    if (file) {
        const char *basename = strrchr(file, PATH_SEP);
        if (!basename) basename = strrchr(file, '/');
        if (!basename) basename = file;
        else basename++;
        
        strncpy(vars->name, basename, BACKUP_MAX_NAME - 1);
        char *dot = strrchr(vars->name, '.');
        if (dot) *dot = '\0';
    }
    
    /* Date and time */
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    strftime(vars->date, sizeof(vars->date), "%Y-%m-%d", tm);
    strftime(vars->time_str, sizeof(vars->time_str), "%H%M%S", tm);
}

/* Substitute variables in command template */
int backup_substitute(char *out, size_t out_size, 
                      const char *template_cmd, BackupVars *vars) {
    if (!out || !template_cmd || !vars) return -1;
    
    const char *src = template_cmd;
    char *dst = out;
    char *end = out + out_size - 1;
    
    while (*src && dst < end) {
        if (*src == '{') {
            const char *close = strchr(src, '}');
            if (close) {
                size_t var_len = close - src - 1;
                const char *value = NULL;
                
                if (strncmp(src + 1, "archive", var_len) == 0) {
                    value = vars->archive;
                } else if (strncmp(src + 1, "p", var_len) == 0) {
                    value = vars->project;
                } else if (strncmp(src + 1, "e", var_len) == 0) {
                    value = vars->file;
                } else if (strncmp(src + 1, "n", var_len) == 0) {
                    value = vars->name;
                } else if (strncmp(src + 1, "b", var_len) == 0) {
                    value = vars->bindir;
                } else if (strncmp(src + 1, "date", var_len) == 0) {
                    value = vars->date;
                } else if (strncmp(src + 1, "time", var_len) == 0) {
                    value = vars->time_str;
                }
                
                if (value) {
                    size_t val_len = strlen(value);
                    if (dst + val_len < end) {
                        strcpy(dst, value);
                        dst += val_len;
                    }
                    src = close + 1;
                    continue;
                }
            }
        }
        *dst++ = *src++;
    }
    *dst = '\0';
    
    return 0;
}

/* Load backup configuration */
int backup_config_load(BackupConfig *cfg, const char *ini_path) {
    if (!cfg || !ini_path) return -1;
    
    memset(cfg, 0, sizeof(BackupConfig));
    strncpy(cfg->ini_path, ini_path, BACKUP_MAX_PATH - 1);
    
    /* Defaults */
    cfg->settings.threshold_mb = 100;
    cfg->settings.interval = 0;
    strcpy(cfg->settings.archive_format, "tar");
    
    FILE *f = fopen(ini_path, "r");
    if (!f) return -1;
    
    char line[1024];
    char section[64] = {0};
    
    while (fgets(line, sizeof(line), f)) {
        str_trim(line);
        if (line[0] == ';' || line[0] == '#' || line[0] == '\0') continue;
        
        if (line[0] == '[') {
            char *end = strchr(line, ']');
            if (end) {
                *end = '\0';
                strncpy(section, line + 1, sizeof(section) - 1);
            }
            continue;
        }
        
        char *eq = strchr(line, '=');
        if (!eq) continue;
        
        *eq = '\0';
        char *key = line;
        char *value = eq + 1;
        str_trim(key);
        str_trim(value);
        
        if (strcmp(section, "settings") == 0) {
            if (strcmp(key, "threshold_mb") == 0) {
                cfg->settings.threshold_mb = atoi(value);
            } else if (strcmp(key, "interval") == 0) {
                cfg->settings.interval = atoi(value);
            } else if (strcmp(key, "archive_format") == 0) {
                strncpy(cfg->settings.archive_format, value, 
                        sizeof(cfg->settings.archive_format) - 1);
            } else if (strcmp(key, "temp_dir") == 0) {
                strncpy(cfg->settings.temp_dir, value, BACKUP_MAX_PATH - 1);
            }
        } else if (strcmp(section, "destinations") == 0) {
            if (cfg->dest_count < BACKUP_MAX_DESTINATIONS) {
                BackupDest *dest = &cfg->destinations[cfg->dest_count++];
                strncpy(dest->name, key, BACKUP_MAX_NAME - 1);
                strncpy(dest->command, value, BACKUP_MAX_CMD - 1);
            }
        } else if (strcmp(section, "schedule") == 0) {
            if (strcmp(key, "destination") == 0) {
                strncpy(cfg->settings.default_dest, value, BACKUP_MAX_NAME - 1);
            }
        }
    }
    
    fclose(f);
    return 0;
}

/* Free backup config (currently nothing to free) */
void backup_config_free(BackupConfig *cfg) {
    (void)cfg;
}

/* Get destination by name */
BackupDest *backup_config_get_dest(BackupConfig *cfg, const char *name) {
    if (!cfg || !name) return NULL;
    
    for (size_t i = 0; i < cfg->dest_count; i++) {
        if (strcmp(cfg->destinations[i].name, name) == 0) {
            return &cfg->destinations[i];
        }
    }
    
    return NULL;
}

/* Execute backup to destination */
int backup_execute(BackupConfig *cfg, const char *dest_name,
                   const char *project_dir, const char *archive_path) {
    BackupDest *dest = backup_config_get_dest(cfg, dest_name);
    if (!dest) return -1;
    
    BackupVars vars;
    backup_vars_init(&vars, archive_path, project_dir, NULL, NULL);
    
    char cmd[1024];
    backup_substitute(cmd, sizeof(cmd), dest->command, &vars);
    
    return system(cmd);
}

/* High-level backup operation */
int backup_project(BackupConfig *cfg, const char *dest_name,
                   const char *project_dir, int include_history) {
    if (!cfg || !dest_name || !project_dir) return -1;
    
    /* Generate archive name */
    char archive_name[BACKUP_MAX_PATH];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d-%H%M%S", tm);
    
    const char *dirname = strrchr(project_dir, PATH_SEP);
    if (!dirname) dirname = strrchr(project_dir, '/');
    if (!dirname) dirname = project_dir;
    else dirname++;
    
    snprintf(archive_name, sizeof(archive_name), "%s-backup-%s.tar",
             dirname, timestamp);
    
    /* Create archive */
    Archive *ar = archive_create(archive_name);
    if (!ar) return -1;
    
    /* Add project files (excluding .tedit-history unless requested) */
    /* For simplicity, add all files in project_dir */
    archive_add_directory(ar, project_dir, "files");
    
    /* Optionally add history files */
    if (include_history) {
        /* History files are .tedit-history alongside source files */
        /* They're already included if in project_dir */
    }
    
    int result = archive_finalize(ar);
    archive_destroy(ar);
    
    if (result != 0) return result;
    
    /* Execute backup destination command */
    return backup_execute(cfg, dest_name, project_dir, archive_name);
}

/* Get default backup.ini path */
void backup_get_default_ini_path(char *out, size_t out_size, const char *exe_dir) {
    snprintf(out, out_size, "%s%cbackup.ini", exe_dir, PATH_SEP);
}


