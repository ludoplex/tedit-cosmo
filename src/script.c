/*
 * script.c - QSE script engine
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "script.h"
#include "util.h"
#include "platform.h"

/* File handles for script */
static FILE *script_files[16] = {0};

int script_init(ScriptContext *ctx) {
    memset(ctx, 0, sizeof(*ctx));
    getcwd(ctx->cwd, sizeof(ctx->cwd));
    return 0;
}

void script_free(ScriptContext *ctx) {
    /* Close any open files */
    for (int i = 0; i < 16; i++) {
        if (script_files[i]) {
            fclose(script_files[i]);
            script_files[i] = NULL;
        }
    }
    
    /* Free string variables */
    for (size_t i = 0; i < ctx->var_count; i++) {
        if (ctx->vars[i].type == VAR_STRING && ctx->vars[i].value.string) {
            free(ctx->vars[i].value.string);
        }
    }
    
    if (ctx->gettext_result) {
        free(ctx->gettext_result);
    }
    
    memset(ctx, 0, sizeof(*ctx));
}

static ScriptVar *find_var(ScriptContext *ctx, const char *name) {
    for (size_t i = 0; i < ctx->var_count; i++) {
        if (strcmp(ctx->vars[i].name, name) == 0) {
            return &ctx->vars[i];
        }
    }
    return NULL;
}

static ScriptVar *create_var(ScriptContext *ctx, const char *name, ScriptVarType type) {
    if (ctx->var_count >= 128) return NULL;
    
    ScriptVar *var = &ctx->vars[ctx->var_count++];
    strncpy(var->name, name, sizeof(var->name) - 1);
    var->type = type;
    var->value.integer = 0;
    return var;
}

int script_fcreate(ScriptContext *ctx, const char *filename) {
    (void)ctx;
    
    /* Find free handle */
    int handle = -1;
    for (int i = 1; i < 16; i++) {
        if (!script_files[i]) {
            handle = i;
            break;
        }
    }
    if (handle < 0) return -1;
    
    script_files[handle] = fopen(filename, "w");
    if (!script_files[handle]) return -1;
    
    return handle;
}

int script_fprint(ScriptContext *ctx, int handle, const char *text) {
    (void)ctx;
    if (handle < 1 || handle >= 16 || !script_files[handle]) return -1;
    fprintf(script_files[handle], "%s\n", text);
    return 0;
}

int script_fclose(ScriptContext *ctx, int handle) {
    (void)ctx;
    if (handle < 1 || handle >= 16 || !script_files[handle]) return -1;
    fclose(script_files[handle]);
    script_files[handle] = NULL;
    return 0;
}

int script_gettext(ScriptContext *ctx, const char *prompt, const char *title, const char *default_val) {
    (void)title;
    
    printf("%s [%s]: ", prompt, default_val ? default_val : "");
    
    char buf[512];
    if (fgets(buf, sizeof(buf), stdin)) {
        str_trim(buf);
        if (buf[0] == '\0' && default_val) {
            strcpy(buf, default_val);
        }
        
        if (ctx->gettext_result) free(ctx->gettext_result);
        ctx->gettext_result = str_dup(buf);
        
        /* Set $0 variable */
        ScriptVar *v = find_var(ctx, "$0");
        if (!v) v = create_var(ctx, "$0", VAR_STRING);
        if (v) {
            if (v->value.string) free(v->value.string);
            v->value.string = str_dup(buf);
        }
        
        return 0;
    }
    return -1;
}

int script_getfolder(ScriptContext *ctx, const char *prompt, const char *title) {
    char path[260];
    printf("%s: ", prompt);
    (void)title;
    
    if (fgets(path, sizeof(path), stdin)) {
        str_trim(path);
        
        if (ctx->gettext_result) free(ctx->gettext_result);
        ctx->gettext_result = str_dup(path);
        
        return 0;
    }
    return -1;
}

int script_run_cmd(ScriptContext *ctx, const char *cmd) {
    (void)ctx;
    return system(cmd);
}

int script_chdir(ScriptContext *ctx, const char *path) {
    if (chdir(path) == 0) {
        strncpy(ctx->cwd, path, sizeof(ctx->cwd) - 1);
        return 0;
    }
    return -1;
}

int script_run_line(ScriptContext *ctx, const char *line) {
    char buf[512];
    strncpy(buf, line, sizeof(buf) - 1);
    char *s = str_trim(buf);
    
    /* Skip empty lines and comments */
    if (s[0] == '\0' || s[0] == ';') return 0;
    
    /* Variable declaration: INTEGER varname */
    if (strncmp(s, "INTEGER ", 8) == 0) {
        create_var(ctx, str_trim(s + 8), VAR_INTEGER);
        return 0;
    }
    
    /* Variable declaration: STRING varname$ */
    if (strncmp(s, "STRING ", 7) == 0) {
        create_var(ctx, str_trim(s + 7), VAR_STRING);
        return 0;
    }
    
    /* End script */
    if (strcmp(s, "end") == 0) {
        return 1; /* Signal end */
    }
    
    /* chdir path */
    if (strncmp(s, "chdir ", 6) == 0) {
        return script_chdir(ctx, str_trim(s + 6));
    }
    
    /* run "command" */
    if (strncmp(s, "run ", 4) == 0) {
        char *cmd = str_trim(s + 4);
        /* Remove quotes if present */
        if (cmd[0] == '"') {
            cmd++;
            char *end = strchr(cmd, '"');
            if (end) *end = '\0';
        }
        return script_run_cmd(ctx, cmd);
    }
    
    /* Handle assignments and function calls - simplified */
    /* TODO: Full parser for complex expressions */
    
    return 0;
}

int script_run_file(ScriptContext *ctx, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg), 
                 "Cannot open script: %s", path);
        ctx->error = 1;
        return -1;
    }
    
    char line[512];
    int line_num = 0;
    
    while (fgets(line, sizeof(line), f)) {
        line_num++;
        int result = script_run_line(ctx, line);
        if (result == 1) break; /* End command */
        if (result < 0) {
            snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                     "Error on line %d", line_num);
            ctx->error = 1;
            fclose(f);
            return -1;
        }
    }
    
    fclose(f);
    return 0;
}

