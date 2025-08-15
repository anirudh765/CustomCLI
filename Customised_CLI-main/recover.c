#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>
#include <limits.h>

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: recover <filename1> [filename2] ...\n");
        return 1;
    }

    // 1. Locate $HOME/.local/share/Trash/files
    const char *home = getenv("HOME");
    if (!home)
    {
        fprintf(stderr, "ERROR: HOME not set\n");
        return 1;
    }

    char trash_dir[PATH_MAX];
    int n = snprintf(trash_dir, sizeof trash_dir,
                     "%s/.local/share/Trash/files", home);
    if (n < 0 || n >= (int)sizeof trash_dir)
    {
        fprintf(stderr, "ERROR: Trash directory path too long\n");
        return 1;
    }

    struct stat st;
    if (stat(trash_dir, &st) != 0 || !S_ISDIR(st.st_mode))
    {
        fprintf(stderr, "ERROR: Trash directory does not exist: %s\n",
                trash_dir);
        return 1;
    }

    // 2. Get current working directory
    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof cwd))
    {
        perror("getcwd");
        return 1;
    }

    // 3. Loop over each filename argument
    for (int i = 1; i < argc; i++)
    {
        
        char *name_copy = strdup(argv[i]);
        if (!name_copy)
        {
            perror("strdup");
            continue;
        }
        char *base = basename(name_copy);

       
        char src[PATH_MAX];
        n = snprintf(src, sizeof src, "%s/%s", trash_dir, base);
        if (n < 0 || n >= (int)sizeof src)
        {
            fprintf(stderr, "ERROR: Source path too long for %s\n", base);
            free(name_copy);
            continue;
        }

        
        if (access(src, F_OK) != 0)
        {
            fprintf(stderr, "File '%s' not found in Trash.\n", base);
            free(name_copy);
            continue;
        }

        
        char dest[PATH_MAX];
        n = snprintf(dest, sizeof dest, "%s/%s", cwd, base);
        if (n < 0 || n >= (int)sizeof dest)
        {
            fprintf(stderr, "ERROR: Destination path too long for %s\n", base);
            free(name_copy);
            continue;
        }

        
        if (access(dest, F_OK) == 0)
        {
            fprintf(stderr, "ERROR: Destination already exists: %s\n", dest);
            free(name_copy);
            continue;
        }

        
        FILE *src_fp = fopen(src, "rb");
        if (!src_fp)
        {
            fprintf(stderr, "Failed to open source '%s': %s\n", src, strerror(errno));
            free(name_copy);
            continue;
        }

        FILE *dest_fp = fopen(dest, "wb");
        if (!dest_fp)
        {
            fprintf(stderr, "Failed to open destination '%s': %s\n", dest, strerror(errno));
            fclose(src_fp);
            free(name_copy);
            continue;
        }

        char buffer[4096];
        size_t bytes;
        while ((bytes = fread(buffer, 1, sizeof(buffer), src_fp)) > 0)
        {
            fwrite(buffer, 1, bytes, dest_fp);
        }

        fclose(src_fp);
        fclose(dest_fp);

        
        if (unlink(src) == 0)
        {
            printf("Recovered '%s' → '%s'\n", base, dest);
        }
        else
        {
            fprintf(stderr, "Copied but failed to delete '%s': %s\n", src, strerror(errno));
        }

        free(name_copy);
    }

    return 0;
}
