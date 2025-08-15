#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

int is_older_than(const char *filepath, int days) {
    struct stat st;
    if (stat(filepath, &st) != 0) return 0;

    time_t now = time(NULL);
    double diff_days = difftime(now, st.st_mtime) / (60 * 60 * 24);

    return diff_days >= days;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: cleanlogs <directory> <days>\n");
        return 1;
    }

    const char *path = argv[1];
    int days = atoi(argv[2]);

    DIR *dir = opendir(path);
    if (!dir) {
        perror("opendir failed");
        return 1;
    }

    struct dirent *entry;
    char fullpath[512];

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) continue;
        if (entry->d_name[0] == '.') continue;

        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

        if (is_older_than(fullpath, days)) {
            if (unlink(fullpath) == 0) {
                printf("Deleted: %s\n", fullpath);
            } else {
                printf("Error deleting %s: %s\n", fullpath, strerror(errno));
            }
        }
    }

    closedir(dir);
    return 0;
}