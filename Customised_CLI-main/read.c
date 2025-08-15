#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: readfile <file1> [file2 ...]\n");
        return 1;
    }

    char buffer[BUFFER_SIZE];

    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);
        if (fd < 0) {
            printf("Could not open '%s': %s\n", argv[i], strerror(errno));
            continue;
        }

        ssize_t bytes;
        while ((bytes = read(fd, buffer, BUFFER_SIZE)) > 0) {
            write(STDOUT_FILENO, buffer, bytes);
        }

        close(fd);
    }

    return 0;
}