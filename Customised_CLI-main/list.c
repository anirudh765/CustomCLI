#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

// From <linux/dirent.h>
struct linux_dirent64 {
    ino64_t        d_ino;
    off64_t        d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
    char           d_name[];
};

int main(int argc, char *argv[]) {
    const char *path = (argc > 1 ? argv[1] : ".");
    
    int fd = syscall(SYS_openat, AT_FDCWD, path, O_RDONLY | O_DIRECTORY, 0);
    if (fd < 0) {
        fprintf(stderr, "Error opening '%s': %s\n", path, strerror(errno));
        return 1;
    }

    // Buffer for getdents64
    const size_t BUF_SIZE = 16 * 1024;
    char *buf = malloc(BUF_SIZE);
    if (!buf) {
        perror("malloc");
        close(fd);
        return 1;
    }

    // Read directory entries
    while (1) {
        int nread = syscall(SYS_getdents64, fd, buf, BUF_SIZE);
        if (nread < 0) {
            perror("getdents64");
            free(buf);
            close(fd);
            return 1;
        }
        if (nread == 0)  
            break;

        int bpos = 0;
        while (bpos < nread) {
            struct linux_dirent64 *d = (void *)(buf + bpos);
        
            if (strcmp(d->d_name, ".") != 0 && strcmp(d->d_name, "..") != 0) {
            
                syscall(SYS_write, STDOUT_FILENO, d->d_name, strlen(d->d_name));
                syscall(SYS_write, STDOUT_FILENO, "\n", 1);
            }
            bpos += d->d_reclen;
        }
    }

    free(buf);
    close(fd);
    return 0;
}
