#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "cli/internal.h"

/* Writes the one-line usage. */
static void usage(void)
{
    fprintf(stderr, "usage: ekoc build <file.eko> [-o <out>]\n");
}

/* Resolves the directory holding the compiler's own tree: the running
   executable's directory, one level up when that directory is build/.
   Falls back to "." when the executable path cannot be read. */
static void tree_root(char *buf, size_t cap)
{
    ssize_t n = readlink("/proc/self/exe", buf, cap - 1);
    char *slash;

    buf[0] = '\0';
    if (n <= 0) {
        strcpy(buf, ".");
        return;
    }
    buf[n] = '\0';
    slash = strrchr(buf, '/');
    if (slash == NULL) {
        strcpy(buf, ".");
        return;
    }
    *slash = '\0';
    slash = strrchr(buf, '/');
    if (slash != NULL && strcmp(slash + 1, "build") == 0) {
        *slash = '\0';
    }
    if (buf[0] == '\0') {
        strcpy(buf, "/");
    }
}

int main(int argc, char **argv)
{
    char root[4096];
    const char *file = NULL;
    const char *out = NULL;
    int i;

    if (argc < 2 || strcmp(argv[1], "build") != 0) {
        usage();
        return 2;
    }
    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (out != NULL || i + 1 >= argc) {
                usage();
                return 2;
            }
            out = argv[++i];
        } else if (argv[i][0] == '-' && argv[i][1] != '\0') {
            usage();
            return 2;
        } else if (file == NULL) {
            file = argv[i];
        } else {
            usage();
            return 2;
        }
    }
    if (file == NULL) {
        usage();
        return 2;
    }
    tree_root(root, sizeof root);
    return cli_build(file, out, root);
}
