#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

static int is_directory(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

static void build_dest_path(const char *dest_dir,
                            const char *src_name,
                            char *out_path, size_t out_sz)
{
    const char *base = basename((char*)src_name);
    if (snprintf(out_path, out_sz, "%s/%s", dest_dir, base) >= (int)out_sz) {
        fprintf(stderr, "Path too long\n");
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage:\n"
                        "  mv <source>... <directory>\n"
                        "  mv <source> <destination>\n");
        return 1;
    }

    const char *dest = argv[argc-1];

    if (argc > 3) {
        // mv file1 file2 ... dir
        if (!is_directory(dest)) {
            fprintf(stderr, "Error: when moving multiple files, '%s' is not a directory\n", dest);
            return 1;
        }
        for (int i = 1; i < argc-1; i++) {
            char target[PATH_MAX];
            build_dest_path(dest, argv[i], target, sizeof target);
            if (rename(argv[i], target) != 0) {
                fprintf(stderr, "Error moving '%s' → '%s': %s\n",
                        argv[i], target, strerror(errno));
            }else {
                printf("Moved '%s' → '%s'\n", argv[i], target);
            }
        }
    } else {
        // argc == 3: either rename or move into directory
        const char *src = argv[1];
        char target[PATH_MAX];

        if (is_directory(dest)) {
            build_dest_path(dest, src, target, sizeof target);
        } else {
            // treat dest as file name
            strncpy(target, dest, sizeof target);
            target[sizeof target - 1] = '\0';
        }

        if (rename(src, target) == 0) {
            printf("Moved '%s' → '%s'\n", src, target);
        } else {
            fprintf(stderr, "Error moving '%s' → '%s': %s\n",
                    src, target, strerror(errno));
            return 1;
        }
        
    }
    return 0;
}
