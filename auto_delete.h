#ifndef AUTO_DELETE_H
#define AUTO_DELETE_H

#include <sqlite3.h>

#define DEFAULT_RETENTION_SECS 60  // Default retention time in seconds

typedef struct {
    const char* home_dir;
    char* recycle_bin;
    char* db_path;
} AutoDeleteSystem;

// Function declarations
int init_system(AutoDeleteSystem* system);
void cleanup_system(AutoDeleteSystem* system);
int init_database(AutoDeleteSystem* system);
char* delete_file(AutoDeleteSystem* system, const char* file_path, int retention_secs);
char* list_recycled(AutoDeleteSystem* system);
char* restore_file(AutoDeleteSystem* system, int file_id);
char* purge_expired(AutoDeleteSystem* system);

// Helper functions
int create_directory(const char* path);
char* path_join(const char* path1, const char* path2);
char* get_abs_path(const char* path);
char* get_basename(const char* path);
char* get_dirname(const char* path);
char* get_extension(const char* path);
char* format_string(const char* format, ...);

#endif