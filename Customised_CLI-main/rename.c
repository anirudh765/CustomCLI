#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: rename <old_name> <new_name>\n");
        return 1;
    }

    if (rename(argv[1], argv[2]) == 0) {
        printf("Renamed '%s' to '%s'\n", argv[1], argv[2]);
    } else {
        printf("Error renaming: %s\n", strerror(errno));
        return 1;
    }

    return 0;
}