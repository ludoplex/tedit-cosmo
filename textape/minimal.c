/*
 * Minimal APE Template
 * Build: cosmocc -Os -o app.com app.c
 */
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <args>\n", argv[0]);
        return 1;
    }
    
    /* Your code here */
    
    return 0;
}

