/*
 * script.h - QSE script engine
 */
#ifndef TEDIT_SCRIPT_H
#define TEDIT_SCRIPT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Script variable types */
typedef enum ScriptVarType {
    VAR_INTEGER,
    VAR_STRING
} ScriptVarType;

typedef struct ScriptVar {
    char name[64];
    ScriptVarType type;
    union {
        long integer;
        char *string;
    } value;
} ScriptVar;

typedef struct ScriptContext {
    ScriptVar vars[128];
    size_t var_count;
    char cwd[260];
    char *gettext_result;
    int error;
    char error_msg[256];
} ScriptContext;

/* Script execution */
int script_init(ScriptContext *ctx);
void script_free(ScriptContext *ctx);

int script_run_file(ScriptContext *ctx, const char *path);
int script_run_line(ScriptContext *ctx, const char *line);

/* Built-in functions */
int script_fcreate(ScriptContext *ctx, const char *filename);
int script_fprint(ScriptContext *ctx, int handle, const char *text);
int script_fclose(ScriptContext *ctx, int handle);
int script_gettext(ScriptContext *ctx, const char *prompt, const char *title, const char *default_val);
int script_getfolder(ScriptContext *ctx, const char *prompt, const char *title);
int script_run_cmd(ScriptContext *ctx, const char *cmd);
int script_chdir(ScriptContext *ctx, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* TEDIT_SCRIPT_H */

