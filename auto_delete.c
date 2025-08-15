
/* auto_delete.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <libgen.h>
#include <limits.h>
#include <errno.h>
#include <stdarg.h>
#include <syslog.h>
#include <sqlite3.h>
#include "auto_delete.h"

int init_system(AutoDeleteSystem* system) {
    system->home_dir = getenv("HOME");
    if (system->home_dir == NULL) {
        fprintf(stderr, "Error: Cannot determine home directory\n");
        return 0;
    }
   
    system->recycle_bin = path_join(system->home_dir, ".recycle_bin");
    if (system->recycle_bin == NULL) {
        return 0;
    }
    
    if (!create_directory(system->recycle_bin)) {
        free(system->recycle_bin);
        return 0;
    }
    
    system->db_path = path_join(system->recycle_bin, "tracking.db");
    if (system->db_path == NULL) {
        free(system->recycle_bin);
        return 0;
    }
    
    if (!init_database(system)) {
        free(system->recycle_bin);
        free(system->db_path);
        return 0;
    }
    
    return 1;
}

void cleanup_system(AutoDeleteSystem* system) {
    if (system->recycle_bin) {
        free(system->recycle_bin);
    }
    if (system->db_path) {
        free(system->db_path);
    }
}

int init_database(AutoDeleteSystem* system) {
    sqlite3* db;
    char* err_msg = NULL;
    int rc;
    
    rc = sqlite3_open(system->db_path, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 0;
    }
    
    // Create table if it doesn't exist
    const char* sql = "CREATE TABLE IF NOT EXISTS deleted_files ("
                      "id INTEGER PRIMARY KEY,"
                      "original_path TEXT,"
                      "delete_timestamp INTEGER,"
                      "scheduled_deletion INTEGER,"
                      "file_type TEXT"
                      ");";
    
    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return 0;
    }
    
    sqlite3_close(db);
    return 1;
}

char* delete_file(AutoDeleteSystem* system, const char* file_path, int retention_secs) {
    char* abs_path = get_abs_path(file_path);
    if (abs_path == NULL) {
        return format_string("Error: Could not get absolute path for %s", file_path);
    }
    
    // Check if file exists
    struct stat st;
    if (stat(abs_path, &st) != 0) {
        char* result = format_string("Error: File %s does not exist", file_path);
        free(abs_path);
        return result;
    }
    
    // Generate unique filename in recycle bin
    char* filename = get_basename(abs_path);
    if (filename == NULL) {
        free(abs_path);
        return strdup("Error: Could not get basename");
    }
    
    time_t timestamp = time(NULL);
    char* unique_name = format_string("%ld_%s", timestamp, filename);
    free(filename);
    
    if (unique_name == NULL) {
        free(abs_path);
        return strdup("Error: Could not create unique filename");
    }
    
    char* recycled_path = path_join(system->recycle_bin, unique_name);
    free(unique_name);
    
    if (recycled_path == NULL) {
        free(abs_path);
        return strdup("Error: Could not create recycled path");
    }
    
    // Move file to recycle bin
    if (rename(abs_path, recycled_path) != 0) {
        char* result = format_string("Error moving file: %s", strerror(errno));
        free(abs_path);
        free(recycled_path);
        return result;
    }
    
    // Add to database
    sqlite3* db;
    int rc = sqlite3_open(system->db_path, &db);
    if (rc != SQLITE_OK) {
        free(abs_path);
        free(recycled_path);
        return format_string("Error opening database: %s", sqlite3_errmsg(db));
    }
    
    char* file_type = get_extension(abs_path);
    if (file_type == NULL) {
        file_type = strdup("");
    }
    
    time_t scheduled_deletion = timestamp + retention_secs;
    
    char* sql = sqlite3_mprintf(
        "INSERT INTO deleted_files (original_path, delete_timestamp, scheduled_deletion, file_type) "
        "VALUES ('%q', %ld, %ld, '%q')",
        abs_path, timestamp, scheduled_deletion, file_type
    );
    
    char* err_msg = NULL;
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    sqlite3_free(sql);
    free(file_type);
    
    if (rc != SQLITE_OK) {
        char* result = format_string("Error adding to database: %s", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        free(abs_path);
        free(recycled_path);
        return result;
    }
    
    sqlite3_close(db);
    free(recycled_path);
    
    char* result = format_string("File %s moved to recycle bin. Will be deleted after %d secs.",
                                file_path, retention_secs);
    free(abs_path);
    return result;
}

char* list_recycled(AutoDeleteSystem* system) {
    sqlite3* db;
    int rc = sqlite3_open(system->db_path, &db);
    if (rc != SQLITE_OK) {
        return format_string("Error opening database: %s", sqlite3_errmsg(db));
    }
    
    const char* sql = "SELECT id, original_path, delete_timestamp, scheduled_deletion "
                     "FROM deleted_files";
    
    sqlite3_stmt* stmt;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_close(db);
        return format_string("Error preparing SQL: %s", sqlite3_errmsg(db));
    }
    
    int row_count = 0;
    size_t estimated_size = 100; 
    
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        row_count++;
        estimated_size += strlen((const char*)sqlite3_column_text(stmt, 1)) + 100; 
    }
    
    sqlite3_reset(stmt);
    
    if (row_count == 0) {
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return strdup("No files in recycle bin");
    }
    
    // Allocate buffer for the result
    char* result = (char*)malloc(estimated_size);
    if (result == NULL) {
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return strdup("Error: Memory allocation failed");
    }
    
    // Create header
    strcpy(result, "ID | Original Path | Deleted On | Scheduled Deletion\n");
    strcat(result, "------------------------------------------------------------\n");
    
    size_t pos = strlen(result);
    
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        const char* path = (const char*)sqlite3_column_text(stmt, 1);
        time_t del_time = (time_t)sqlite3_column_int64(stmt, 2);
        time_t sched_time = (time_t)sqlite3_column_int64(stmt, 3);
        
        char del_date[20], sched_date[20];
        struct tm* tm_info;
        
        tm_info = localtime(&del_time);
        strftime(del_date, sizeof(del_date), "%Y-%m-%d %H:%M", tm_info);
        
        tm_info = localtime(&sched_time);
        strftime(sched_date, sizeof(sched_date), "%Y-%m-%d %H:%M", tm_info);
        
        int written = snprintf(result + pos, estimated_size - pos, 
                              "%d | %s | %s | %s\n", 
                              id, path, del_date, sched_date);
        
        if (written < 0 || written >= estimated_size - pos) {
            strcat(result, "... (truncated)");
            break;
        }
        
        pos += written;
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    return result;
}

char* restore_file(AutoDeleteSystem* system, int file_id) {
    sqlite3* db;
    int rc = sqlite3_open(system->db_path, &db);
    if (rc != SQLITE_OK) {
        return format_string("Error opening database: %s", sqlite3_errmsg(db));
    }
    
    char* sql = sqlite3_mprintf(
        "SELECT original_path, delete_timestamp FROM deleted_files WHERE id = %d",
        file_id
    );
    
    sqlite3_stmt* stmt;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_free(sql);
    
    if (rc != SQLITE_OK) {
        sqlite3_close(db);
        return format_string("Error preparing SQL: %s", sqlite3_errmsg(db));
    }
    
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return format_string("Error: No file with ID %d in recycle bin", file_id);
    }
    
    const char* original_path = (const char*)sqlite3_column_text(stmt, 0);
    time_t timestamp = (time_t)sqlite3_column_int64(stmt, 1);
    
    char* original_path_copy = strdup(original_path);
    if (original_path_copy == NULL) {
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return strdup("Error: Memory allocation failed");
    }
    
    char* filename = get_basename(original_path_copy);
    if (filename == NULL) {
        free(original_path_copy);
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return strdup("Error: Could not get basename");
    }
    
    char* recycled_name = format_string("%ld_%s", timestamp, filename);
    free(filename);
    
    if (recycled_name == NULL) {
        free(original_path_copy);
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return strdup("Error: Could not create filename");
    }
    
    char* recycled_path = path_join(system->recycle_bin, recycled_name);
    free(recycled_name);
    
    if (recycled_path == NULL) {
        free(original_path_copy);
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return strdup("Error: Could not create recycled path");
    }
    
    struct stat st;
    if (stat(recycled_path, &st) != 0) {
        sql = sqlite3_mprintf("DELETE FROM deleted_files WHERE id = %d", file_id);
        sqlite3_exec(db, sql, NULL, NULL, NULL);
        sqlite3_free(sql);
        
        char* result = format_string("Error: File %s no longer exists in recycle bin", filename);
        free(original_path_copy);
        free(recycled_path);
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return result;
    }
    
    // Ensure the directory exists
    char* original_dir = get_dirname(original_path_copy);
    if (original_dir == NULL) {
        free(original_path_copy);
        free(recycled_path);
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return strdup("Error: Could not get directory name");
    }
    
    if (!create_directory(original_dir)) {
        char* result = format_string("Error: Could not create directory %s", original_dir);
        free(original_path_copy);
        free(recycled_path);
        free(original_dir);
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return result;
    }
    free(original_dir);
    
    char* target_path = strdup(original_path_copy);
    if (target_path == NULL) {
        free(original_path_copy);
        free(recycled_path);
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return strdup("Error: Memory allocation failed");
    }
    
    if (stat(target_path, &st) == 0) {
        char* last_dot = strrchr(target_path, '.');
        if (last_dot != NULL) {
            *last_dot = '\0'; 
            char* new_path = format_string("%s_restored%s", target_path, last_dot);
            free(target_path);
            target_path = new_path;
        } else {
            char* new_path = format_string("%s_restored", target_path);
            free(target_path);
            target_path = new_path;
        }
    }

    if (rename(recycled_path, target_path) != 0) {
        char* result = format_string("Error restoring file: %s", strerror(errno));
        free(original_path_copy);
        free(recycled_path);
        free(target_path);
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return result;
    }
    
    sql = sqlite3_mprintf("DELETE FROM deleted_files WHERE id = %d", file_id);
    sqlite3_exec(db, sql, NULL, NULL, NULL);
    sqlite3_free(sql);
    
    char* result = format_string("File restored to %s", target_path);
    
    free(original_path_copy);
    free(recycled_path);
    free(target_path);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    return result;
}

char* purge_expired(AutoDeleteSystem* system) {
    sqlite3* db;
    int rc = sqlite3_open(system->db_path, &db);
    if (rc != SQLITE_OK) {
        syslog(LOG_ERR, "Error opening database: %s", sqlite3_errmsg(db));
        return format_string("Error opening database: %s", sqlite3_errmsg(db));
    }
    
    time_t current_time = time(NULL);
    syslog(LOG_INFO, "Current time: %ld", current_time);
    
    // Find expired files
    char* sql = sqlite3_mprintf(
        "SELECT id, delete_timestamp, original_path, scheduled_deletion FROM deleted_files WHERE scheduled_deletion <= %ld",
        current_time
    );
    
    syslog(LOG_INFO, "Executing SQL: %s", sql);
    
    sqlite3_stmt* stmt;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_free(sql);
    
    if (rc != SQLITE_OK) {
        syslog(LOG_ERR, "Error preparing SQL: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return format_string("Error preparing SQL: %s", sqlite3_errmsg(db));
    }
    
    int purged_count = 0;
    int failed_count = 0;
    
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int file_id = sqlite3_column_int(stmt, 0);
        time_t timestamp = (time_t)sqlite3_column_int64(stmt, 1);
        const char* original_path = (const char*)sqlite3_column_text(stmt, 2);
        time_t scheduled_time = (time_t)sqlite3_column_int64(stmt, 3);
        
        syslog(LOG_INFO, "Found expired file: ID=%d, Path=%s, Scheduled=%ld (now=%ld)", 
               file_id, original_path, scheduled_time, current_time);
        
        char* filename = get_basename(original_path);
        if (filename == NULL) {
            syslog(LOG_ERR, "Failed to get basename for %s", original_path);
            failed_count++;
            continue;
        }
        
        char* recycled_name = format_string("%ld_%s", timestamp, filename);
        free(filename);
        
        if (recycled_name == NULL) {
            syslog(LOG_ERR, "Failed to format recycled name");
            failed_count++;
            continue;
        }
        
        char* recycled_path = path_join(system->recycle_bin, recycled_name);
        free(recycled_name);
        
        if (recycled_path == NULL) {
            syslog(LOG_ERR, "Failed to join paths");
            failed_count++;
            continue;
        }
        
        syslog(LOG_INFO, "Attempting to delete: %s", recycled_path);
        
        if (unlink(recycled_path) == 0) {
            syslog(LOG_INFO, "Successfully deleted file: %s", recycled_path);
        
            char* delete_sql = sqlite3_mprintf("DELETE FROM deleted_files WHERE id = %d", file_id);
            sqlite3_exec(db, delete_sql, NULL, NULL, NULL);
            sqlite3_free(delete_sql);
            purged_count++;
        } else if (errno == ENOENT) {
            syslog(LOG_WARNING, "File doesn't exist, removing from DB: %s", recycled_path);
            char* delete_sql = sqlite3_mprintf("DELETE FROM deleted_files WHERE id = %d", file_id);
            sqlite3_exec(db, delete_sql, NULL, NULL, NULL);
            sqlite3_free(delete_sql);
            purged_count++;
        } else {
            syslog(LOG_ERR, "Failed to delete %s: %s", recycled_path, strerror(errno));
            failed_count++;
        }
        
        free(recycled_path);
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    if (purged_count == 0 && failed_count == 0) {
        return strdup("No expired files to purge");
    } else {
        return format_string("Purged %d expired files, failed to purge %d files", 
                           purged_count, failed_count);
    }
}


int create_directory(const char* path) {
    struct stat st;
    
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return 1; 
        } else {
            fprintf(stderr, "Error: %s exists but is not a directory\n", path);
            return 0;
        }
    }
    
    if (mkdir(path, 0755) != 0) {
        fprintf(stderr, "Error creating directory %s: %s\n", path, strerror(errno));
        return 0;
    }
    
    return 1;
}

char* path_join(const char* path1, const char* path2) {
    size_t len1 = strlen(path1);
    size_t len2 = strlen(path2);
    size_t len = len1 + len2 + 2; 
    char* result = (char*)malloc(len);
    if (result == NULL) {
        return NULL;
    }
    
    strcpy(result, path1);
    
    if (len1 > 0 && path1[len1-1] != '/') {
        strcat(result, "/");
    }
    
    strcat(result, path2);
    return result;
}

char* get_abs_path(const char* path) {
    char* buffer = (char*)malloc(PATH_MAX);
    if (buffer == NULL) {
        return NULL;
    }
    
    if (realpath(path, buffer) == NULL) {
        free(buffer);
        return NULL;
    }
    
    return buffer;
}

char* get_basename(const char* path) {
    char* path_copy = strdup(path);
    if (path_copy == NULL) {
        return NULL;
    }
    
    char* base = basename(path_copy);
    char* result = strdup(base);
    free(path_copy);
    
    return result;
}

char* get_dirname(const char* path) {
    char* path_copy = strdup(path);
    if (path_copy == NULL) {
        return NULL;
    }
    
    char* dir = dirname(path_copy);
    char* result = strdup(dir);
    free(path_copy);
    
    return result;
}

char* get_extension(const char* path) {
    char* last_dot = strrchr(path, '.');
    if (last_dot == NULL) {
        return strdup("");
    }
    
    return strdup(last_dot);
}

char* format_string(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    // Calculate the required buffer size
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(NULL, 0, format, args_copy) + 1; // +1 for null terminator
    va_end(args_copy);
    
    if (size < 0) {
        va_end(args);
        return NULL;
    }
    
    char* buffer = (char*)malloc(size);
    if (buffer == NULL) {
        va_end(args);
        return NULL;
    }
    
    vsnprintf(buffer, size, format, args);
    va_end(args);
    
    return buffer;
}

