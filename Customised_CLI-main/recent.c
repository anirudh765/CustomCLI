#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>      
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <limits.h>      
#include <errno.h>

int main(int argc, char *argv[]) {

    if (argc > 1) {
        printf("Usage: recent\n");
        printf("Lists regular (non-hidden) files in the current directory\n");
        printf("that were modified in the last 24 hours.\n");
        printf("\nExample:\n  ./recent\n");
        return 1;
    }

    char *cwd = getcwd(NULL, 0);
    if (!cwd) {
        perror("getcwd");
        return 1;
    }

    DIR *dir = opendir(cwd);
    if (!dir) {
        perror("opendir");
        free(cwd);
        return 1;
    }

    printf("Recently modified files in %s:\n", cwd);

    time_t now = time(NULL);
    if (now == (time_t)-1) {
        perror("time");
        closedir(dir);
        free(cwd);
        return 1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        
        if (entry->d_name[0] == '.') 
            continue;

        
        char fullpath[PATH_MAX];
        int n = snprintf(fullpath, sizeof(fullpath), "%s/%s", cwd, entry->d_name);
        if (n < 0 || n >= (int)sizeof(fullpath)) {
            fprintf(stderr, "Path too long: %s/%s\n", cwd, entry->d_name);
            continue;
        }

        struct stat info;
        if (stat(fullpath, &info) != 0) {
            fprintf(stderr, "stat(%s) failed: %s\n", fullpath, strerror(errno));
            continue;
        }

   
        if (!S_ISREG(info.st_mode)) 
            continue;

      
        double secs = difftime(now, info.st_mtime);
        if (secs < 0)   secs = 0;   
        if (secs < 86400.0) {
            char mod_time[64];
            struct tm *mt = localtime(&info.st_mtime);
            if (!mt) {
                perror("localtime");
                continue;
            }
            if (!strftime(mod_time, sizeof(mod_time),
                          "%Y-%m-%d %H:%M:%S", mt)) {
                strcpy(mod_time, "Unknown time");
            }

            printf("  %s  (modified: %s)\n", entry->d_name, mod_time);
        }
    }

    closedir(dir);
    free(cwd);
    return 0;
}
