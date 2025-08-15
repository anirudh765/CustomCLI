/* main.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "auto_delete.h"

void print_usage() {
    printf("Usage: auto_delete [delete|list|restore|purge] [args]\n");
    printf("Commands:\n");
    printf("  delete <file_path> [retention_seconds] - Move file to recycle bin\n");
    printf("  list                               - List files in recycle bin\n");
    printf("  restore <file_id>                  - Restore file from recycle bin\n");
    printf("  purge                              - Remove expired files\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }
    
    AutoDeleteSystem system;
    if (!init_system(&system)) {
        fprintf(stderr, "Error initializing auto delete system\n");
        return 1;
    }
    
    char* result = NULL;
    const char* command = argv[1];
    
    if (strcmp(command, "delete") == 0) {
        if (argc < 3) {
            printf("Usage: auto_delete delete <file_path> [retention_seconds]\n");
            cleanup_system(&system);
            return 1;
        }
        
        int retention_secs = 60; // Default
        if (argc >= 4) {
            char* end;
            retention_secs = (int)strtol(argv[3], &end, 10);
            if (*end != '\0') {
                printf("Error: retention must be a number\n");
                cleanup_system(&system);
                return 1;
            }
        }
        
        result = delete_file(&system, argv[2], retention_secs);
    } 
    else if (strcmp(command, "list") == 0) {
        result = list_recycled(&system);
    } 
    else if (strcmp(command, "restore") == 0) {
        if (argc < 3) {
            printf("Usage: auto_delete restore <file_id>\n");
            cleanup_system(&system);
            return 1;
        }
        
        char* end;
        int file_id = (int)strtol(argv[2], &end, 10);
        if (*end != '\0') {
            printf("Error: file_id must be a number\n");
            cleanup_system(&system);
            return 1;
        }
        
        result = restore_file(&system, file_id);
    } 
    else if (strcmp(command, "purge") == 0) {
        result = purge_expired(&system);
    } 
    else {
        printf("Unknown command: %s\n", command);
        print_usage();
        cleanup_system(&system);
        return 1;
    }
    
    if (result != NULL) {
        printf("%s\n", result);
        free(result);
    }
    
    cleanup_system(&system);
    return 0;
}