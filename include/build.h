/*
 * build.h - Build system integration
 */
#ifndef TEDIT_BUILD_H
#define TEDIT_BUILD_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BuildConfig {
    char build_cmd[512];
    char run_cmd[512];
    char clean_cmd[512];
    char assemble_cmd[512];
} BuildConfig;

void build_config_defaults(BuildConfig *cfg);
int build_config_load(BuildConfig *cfg, const char *path);

int build_run_command(const char *cmd);
int build_run_command_capture(const char *cmd, char *output, size_t max);

#ifdef __cplusplus
}
#endif

#endif /* TEDIT_BUILD_H */

