/*
 * build.c - Build system integration
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "build.h"
#include "util.h"

void build_config_defaults(BuildConfig *cfg) {
    strcpy(cfg->build_cmd, "cosmocc -O2 -o {out} {in}");
    strcpy(cfg->run_cmd, "./{out}");
    strcpy(cfg->clean_cmd, "rm -f {out}");
    strcpy(cfg->assemble_cmd, "");
}

int build_config_load(BuildConfig *cfg, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        str_trim(line);
        if (line[0] == ';' || line[0] == '#' || line[0] == '[') continue;
        
        char *eq = strchr(line, '=');
        if (!eq) continue;
        
        *eq = '\0';
        char *key = str_trim(line);
        char *val = str_trim(eq + 1);
        
        if (strcmp(key, "build_cmd") == 0) {
            strncpy(cfg->build_cmd, val, sizeof(cfg->build_cmd) - 1);
        } else if (strcmp(key, "run_cmd") == 0) {
            strncpy(cfg->run_cmd, val, sizeof(cfg->run_cmd) - 1);
        } else if (strcmp(key, "clean_cmd") == 0) {
            strncpy(cfg->clean_cmd, val, sizeof(cfg->clean_cmd) - 1);
        } else if (strcmp(key, "assemble_cmd") == 0) {
            strncpy(cfg->assemble_cmd, val, sizeof(cfg->assemble_cmd) - 1);
        }
    }
    
    fclose(f);
    return 0;
}

int build_run_command(const char *cmd) {
    printf("$ %s\n", cmd);
    int result = system(cmd);
    printf("\n[Exit code: %d]\n", result);
    return result;
}

int build_run_command_capture(const char *cmd, char *output, size_t max) {
    FILE *p = popen(cmd, "r");
    if (!p) return -1;
    
    size_t total = 0;
    char buf[256];
    while (fgets(buf, sizeof(buf), p) && total + strlen(buf) < max) {
        strcpy(output + total, buf);
        total += strlen(buf);
    }
    output[total] = '\0';
    
    int status = pclose(p);
    return status;
}

