#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define BUFFER_SIZE 4096

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: copyfile <source> <destination>\n");
        return 1;
    }

    int source_fd = open(argv[1], O_RDONLY);
    if (source_fd < 0) {
        perror("Error opening source file");
        return 1;
    }

    int dest_fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dest_fd < 0) {
        perror("Error creating destination file");
        close(source_fd);
        return 1;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes;
    while ((bytes = read(source_fd, buffer, BUFFER_SIZE)) > 0) {
        if (write(dest_fd, buffer, bytes) != bytes) {
            perror("Error writing to destination");
            close(source_fd);
            close(dest_fd);
            return 1;
        }
    }

    close(source_fd);
    close(dest_fd);

    printf("Copied '%s' to '%s'\n", argv[1], argv[2]);
    return 0;
}