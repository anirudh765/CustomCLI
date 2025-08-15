#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: createfile <file1> [file2] ...\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        const char *path = argv[i];

        // Create file if it doesn't exist (open with O_CREAT | O_WRONLY)
        int fd = syscall(SYS_open, path, O_CREAT | O_WRONLY, 0666);
        if (fd < 0) {
            fprintf(stderr, "Error creating file '%s': %s\n", path, strerror(errno));
            continue;
        } else {
            printf("Created or opened '%s' successfully\n", path);
        }
        syscall(SYS_close, fd); 

        // Update atime & mtime to current time
        struct timespec times[2];
        times[0].tv_nsec = UTIME_NOW; 
        times[1].tv_nsec = UTIME_NOW; 

        long ret = syscall(SYS_utimensat, AT_FDCWD, path, times, 0);
        if (ret < 0) {
            fprintf(stderr, "Error updating times for '%s': %s\n", path, strerror(errno));
        } else {
            printf("Touched '%s' successfully\n", path);
        }
    }

    return 0;
}
