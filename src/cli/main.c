#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "cli/internal.h"

/* Writes the usage lines, one per command. */
static void usage(void)
{
    fprintf(stderr, "usage: ekoc build <file.eko> [-o <out>]\n"
                    "       ekoc build <file.eko> [--link <lib>]... "
                    "[--shim <file.c>]... [-o <out>]\n");
    fprintf(stderr, "       ekoc check <file.eko>\n");
    fprintf(stderr, "       ekoc test <file.eko>\n");
    fprintf(stderr, "       ekoc where std [<stem>]\n");
    fprintf(stderr, "       ekoc doc [-src] <name>\n");
}

/* Resolves the directory holding the compiler's own tree: the running
   executable's directory, one level up when that directory is build/.
   Falls back to "." when the executable path cannot be read. */
static void tree_root(char *buf, size_t cap)
{
    ssize_t n = readlink("/proc/self/exe", buf, cap - 1);
    char *slash;

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

/* The build argument walk: -o, one input file, and the host-contact
   flags -- each naming one value, repeatable, whose values join the
   link line verbatim: a --link value is the library's own -l spelling
   or path, a --shim value a C source the link compiles in. */
static int run_build(int argc, char **argv, const char *root)
{
    const char *file = NULL;
    const char *out = NULL;
    char extra[1024];
    size_t nextra = 0;
    int i;

    extra[0] = '\0';
    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (out != NULL || i + 1 >= argc) {
                usage();
                return 2;
            }
            out = argv[++i];
        } else if (strcmp(argv[i], "--link") == 0
                   || strcmp(argv[i], "--shim") == 0) {
            const char *val = i + 1 < argc ? argv[i + 1] : NULL;
            size_t len;

            if (val == NULL || nextra + strlen(val) + 2 >= sizeof extra) {
                usage();
                return 2;
            }
            len = sprintf(extra + nextra, "%s'%s'", nextra ? " " : "",
                          val);
            nextra += len;
            i++;
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
    return cli_build(file, out, root, nextra ? extra : NULL);
}

/* The check argument walk: one input file, no options. The command
   compiles nothing, so it needs no tree root. */
static int run_check(int argc, char **argv)
{
    const char *file = NULL;
    int i;

    for (i = 2; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
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
    return cli_check(file);
}

/* The test argument walk: one input file, no options. */
static int run_test(int argc, char **argv, const char *root)
{
    const char *file = NULL;
    int i;

    for (i = 2; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
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
    return cli_test(file, root);
}

int main(int argc, char **argv)
{
    char root[4096];

    if (argc < 2) {
        usage();
        return 2;
    }
    tree_root(root, sizeof root);
    if (strcmp(argv[1], "build") == 0) {
        return run_build(argc, argv, root);
    }
    if (strcmp(argv[1], "check") == 0) {
        return run_check(argc, argv);
    }
    if (strcmp(argv[1], "test") == 0) {
        return run_test(argc, argv, root);
    }
    if (strcmp(argv[1], "where") == 0) {
        return cli_where(root, argc > 2 ? argv[2] : NULL,
                         argc > 3 ? argv[3] : NULL);
    }
    if (strcmp(argv[1], "doc") == 0) {
        int src = argc > 2 && strcmp(argv[2], "-src") == 0;

        if (argc < 2 + (src ? 2 : 1)) {
            usage();
            return 2;
        }
        return cli_doc(root, src, argv[2 + (src ? 1 : 0)]);
    }
    usage();
    return 2;
}
