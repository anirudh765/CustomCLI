#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>

#define MAX_PATH 512

static int mkdir_p(const char *path)
{
    char tmp[MAX_PATH];
    size_t len = strlen(path);
    if (len == 0 || len >= sizeof(tmp))
        return -1;

    strncpy(tmp, path, sizeof(tmp));
    tmp[len] = '\0';

    for (char *p = tmp + 1; *p; p++)
    {
        if (*p == '/')
        {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
                return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
        return -1;
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: temp <file1> [file2] ...\n");
        return 1;
    }

    const char *home = getenv("HOME");
    if (!home)
    {
        fprintf(stderr, "ERROR: HOME not set\n");
        return 1;
    }

    char trash_path[MAX_PATH];
    snprintf(trash_path, sizeof trash_path,
             "%s/.local/share/Trash/files", home);

    
    if (mkdir_p(trash_path) != 0)
    {
        perror("Creating temp directory");
        return 1;
    }

    // Process each file argument
    for (int i = 1; i < argc; i++)
    {
        
        char copy[MAX_PATH];
        if (strlen(argv[i]) >= sizeof copy)
        {
            fprintf(stderr, "Path too long: %s\n", argv[i]);
            continue;
        }
        strcpy(copy, argv[i]);
        char *base = basename(copy);

        char dest[MAX_PATH];
        if (snprintf(dest, sizeof dest, "%s/%s", trash_path, base) >= (int)sizeof dest)
        {
            fprintf(stderr, "Destination path too long for %s\n", base);
            continue;
        }

        FILE *src = fopen(argv[i], "rb");
        if (!src)
        {
            fprintf(stderr, "Failed to open source '%s': %s\n", argv[i], strerror(errno));
            continue;
        }

        FILE *dst = fopen(dest, "wb");
        if (!dst)
        {
            fprintf(stderr, "Failed to open destination '%s': %s\n", dest, strerror(errno));
            fclose(src);
            continue;
        }

        char buf[4096];
        size_t bytes;
        while ((bytes = fread(buf, 1, sizeof(buf), src)) > 0)
        {
            fwrite(buf, 1, bytes, dst);
        }

        fclose(src);
        fclose(dst);

        if (unlink(argv[i]) == 0)
        {
            printf("Moved '%s' → '%s'\n", argv[i], dest);
        }
        else
        {
            fprintf(stderr, "Copied but failed to delete '%s': %s\n", argv[i], strerror(errno));
        }
    }

    return 0;
}
