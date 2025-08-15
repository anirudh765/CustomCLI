#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <fcntl.h>      
#include <errno.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: removefolder <dir1> [dir2] ...\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        long ret = syscall(SYS_unlinkat, AT_FDCWD, argv[i], AT_REMOVEDIR);
        if (ret == 0) {
            printf("Removed directory '%s'\n", argv[i]);
        } else {
            fprintf(stderr,
                    "Error removing '%s': %s\n",
                    argv[i], strerror(errno));
        }
    }

    return 0;
}
