#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: removefile <file1> [file2] ...\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (unlink(argv[i]) == 0) {
            printf("Deleted '%s'\n", argv[i]);
        } else {
            printf("Error deleting '%s': %s\n", argv[i], strerror(errno));
        }
    }

    return 0;
}