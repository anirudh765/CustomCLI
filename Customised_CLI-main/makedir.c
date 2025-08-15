#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: makedir <dir_name>\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        long result = syscall(SYS_mkdir, argv[i], 0755);  

        if (result == 0) {
            printf("Created directory '%s'\n", argv[i]);
        } else {
            if (errno == EEXIST) {
                printf("Directory '%s' already exists.\n", argv[i]);
            } else {
                printf("Error creating '%s': %s\n", argv[i], strerror(errno));
            }
        }
    }

    return 0;
}