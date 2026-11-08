#define _XOPEN_SOURCE 700

#include "internal.h"

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* The directory family of the file-system bundle: the whole-path
   operations on directories, one closure per member, built over the
   emitted unit's Result and IoError identities. */

/* Reads the String argument's bytes. */
static const char *path_arg(ek_val *args, size_t argc)
{
    return argc == 1 ? ek_gen_cstr(args[0]) : "";
}

static int cmp_names(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* The names as a fresh list of Strings, in lexicographic order, the
   dot entries excluded. */
static ek_val list_dir_call(ek_val env, ek_val *args, size_t argc)
{
    DIR *d;
    struct dirent *ent;
    char **names = NULL;
    size_t n = 0, cap = 0, i;
    ek_val list = ek_nil_val();
    const char *dir = path_arg(args, argc);

    (void)env;
    d = argc == 1 ? opendir(dir) : NULL;
    if (d == NULL) {
        int e = errno;

        return ek_gen_err(e == ENOTDIR ? EK_IO_NOT_FILE
                                       : ek_gen_io_kind(e),
                          e == ENOTDIR ? "not a directory" : strerror(e));
    }
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }
        if (n == cap) {
            size_t ncap = cap ? cap * 2 : 16;
            char **next = realloc(names, ncap * sizeof *next);

            if (next == NULL) {
                ek_abort("out of memory");
            }
            names = next;
            cap = ncap;
        }
        names[n] = ek_rt_alloc_zeroed(strlen(ent->d_name) + 1);
        strcpy(names[n], ent->d_name);
        n++;
    }
    closedir(d);
    qsort(names, n, sizeof *names, cmp_names);
    for (i = n; i-- > 0;) {
        ek_val s = ek_val_ref(ek_str_from_cstr(names[i]));
        ek_val next = ek_cons(s, list);

        /* the new cell takes the previous head as its tail, so the
           birth count the builder held moves with it */
        ek_release(s);
        ek_release(list);
        list = next;
        free(names[i]);
    }
    free(names);
    return ek_gen_ok(list);
}

/* Creates one level; an existing path is the Exists reason. */
static ek_val mkdir_call(ek_val env, ek_val *args, size_t argc)
{
    const char *path = path_arg(args, argc);

    (void)env;
    if (argc != 1) {
        return ek_gen_err(EK_IO_OTHER, NULL);
    }
    if (mkdir(path, 0777) != 0) {
        return ek_gen_err(ek_gen_io_kind(errno), strerror(errno));
    }
    return ek_gen_ok(ek_val_unit());
}

/* Creates the whole chain: every prefix is attempted, an existing
   directory tolerated. */
static ek_val mkdir_all_call(ek_val env, ek_val *args, size_t argc)
{
    size_t len, i;
    char *copy;

    (void)env;
    if (argc != 1) {
        return ek_gen_err(EK_IO_OTHER, NULL);
    }
    len = strlen(path_arg(args, argc));
    copy = ek_rt_alloc_zeroed(len + 1);
    memcpy(copy, ek_gen_cstr(args[0]), len);
    for (i = 1; i <= len; i++) {
        if (copy[i] != '/' && copy[i] != '\0') {
            continue;
        }
        {
            char save = copy[i];

            copy[i] = '\0';
            if (mkdir(copy, 0777) != 0 && errno != EEXIST) {
                ek_val r = ek_gen_err(ek_gen_io_kind(errno),
                                      strerror(errno));

                free(copy);
                return r;
            }
            copy[i] = save;
        }
    }
    free(copy);
    return ek_gen_ok(ek_val_unit());
}

static ek_val rm_call(ek_val env, ek_val *args, size_t argc)
{
    (void)env;
    if (argc != 1) {
        return ek_gen_err(EK_IO_OTHER, NULL);
    }
    if (unlink(path_arg(args, argc)) != 0) {
        return ek_gen_err(ek_gen_io_kind(errno),
                          errno == EISDIR ? "is a directory"
                                          : strerror(errno));
    }
    return ek_gen_ok(ek_val_unit());
}

/* Removes an empty directory; the non-empty and the non-directory
   cases carry the Other reason. */
static ek_val rm_dir_call(ek_val env, ek_val *args, size_t argc)
{
    int e;

    (void)env;
    if (argc != 1) {
        return ek_gen_err(EK_IO_OTHER, NULL);
    }
    if (rmdir(path_arg(args, argc)) == 0) {
        return ek_gen_ok(ek_val_unit());
    }
    e = errno;
    return ek_gen_err(e == ENOENT ? EK_IO_NOT_FOUND : EK_IO_OTHER,
                      e == ENOTEMPTY || e == EEXIST ? "directory not empty"
                                                    : strerror(e));
}

static ek_val rename_call(ek_val env, ek_val *args, size_t argc)
{
    (void)env;
    if (argc != 2 || rename(ek_gen_cstr(args[0]), ek_gen_cstr(args[1]))
            != 0) {
        return ek_gen_err(argc != 2 ? EK_IO_OTHER : ek_gen_io_kind(errno),
                          argc != 2 ? NULL : strerror(errno));
    }
    return ek_gen_ok(ek_val_unit());
}

/* The FileInfo record: the size, the wall-clock modification time, and
   the directory flag. */
static ek_val stat_call(ek_val env, ek_val *args, size_t argc)
{
    struct stat st;
    ek_val mt;
    ek_val r;

    (void)env;
    if (argc != 1) {
        return ek_gen_err(EK_IO_OTHER, NULL);
    }
    if (stat(path_arg(args, argc), &st) != 0) {
        return ek_gen_err(ek_gen_io_kind(errno), strerror(errno));
    }
    mt = ek_gen_time((int64_t)st.st_mtime * 1000000000
                     + (int64_t)st.st_mtim.tv_nsec);
    r = ek_gen_ok(ek_gen_file_info((uint64_t)st.st_size, mt,
                                   S_ISDIR(st.st_mode)));
    ek_release(mt);
    return r;
}

static ek_val realpath_call(ek_val env, ek_val *args, size_t argc)
{
    char *resolved;
    ek_val r;

    (void)env;
    if (argc != 1) {
        return ek_gen_err(EK_IO_OTHER, NULL);
    }
    resolved = realpath(path_arg(args, argc), NULL);
    if (resolved == NULL) {
        return ek_gen_err(ek_gen_io_kind(errno), strerror(errno));
    }
    r = ek_gen_ok(ek_val_ref(ek_str_from_cstr(resolved)));
    free(resolved);
    return r;
}

/* One closure per member over the neutral environment. */
static ek_val closure(ek_closure_fn call)
{
    return ek_closure_new(call, ek_val_unit());
}

ek_val ek_fs_list_dir(void)
{
    return closure(list_dir_call);
}

ek_val ek_fs_mkdir(void)
{
    return closure(mkdir_call);
}

ek_val ek_fs_mkdir_all(void)
{
    return closure(mkdir_all_call);
}

ek_val ek_fs_rm(void)
{
    return closure(rm_call);
}

ek_val ek_fs_rm_dir(void)
{
    return closure(rm_dir_call);
}

ek_val ek_fs_rename(void)
{
    return closure(rename_call);
}

ek_val ek_fs_stat(void)
{
    return closure(stat_call);
}

ek_val ek_fs_realpath(void)
{
    return closure(realpath_call);
}
