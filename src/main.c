/*
 * main.c - tedit-cosmo entry point
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "platform.h"

int main(int argc, char **argv) {
    AppState app = {0};
    
    if (app_init(&app) != 0) {
        fprintf(stderr, "Failed to initialize application\n");
        return 1;
    }
    
    /* Open file from command line */
    if (argc > 1) {
        app_open_file(&app, argv[1]);
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

