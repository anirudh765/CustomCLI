#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "email_utils.h"

#define MAX_SUBJECT 256

int main(int argc, char *argv[]) {
    if (argc != 4 || strcmp(argv[2], "to") != 0) {
        fprintf(stderr, "Usage: sendfile <filename> to <email>\n");
        return 1;
    }

    const char *filename = argv[1];
    const char *email = argv[3];

    if (access(filename, F_OK) != 0) {
        perror("File not found");
        return 1;
    }

    char subject[MAX_SUBJECT] = "";
    char choice[10];

    printf("Do you want to add a subject? (yes/no): ");
    if (fgets(choice, sizeof(choice), stdin)) {
        choice[strcspn(choice, "\n")] = 0;
        if (strcmp(choice, "yes") == 0) {
            printf("Enter subject: ");
            if (fgets(subject, sizeof(subject), stdin)) {
                subject[strcspn(subject, "\n")] = 0;
            }
        }
    }

    send_email(filename, email, subject);
    return 0;
}
