#define _XOPEN_SOURCE 700

#include "internal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* The file-system bundle: the closures over the session's whole-path
   operations. The working directory is the root: every relative path
   resolves through it and no further restriction applies. The
   directory operations live in fsdir.c; the fallible closures report
   through the emitted unit's Result and IoError identities. */

/* The open modes in the capability module's variant order, identified
   through the emitted unit's constructor identities. */
static int mode_of(ek_val m)
{
    static const ek_typeinfo *ids[] = {
        &ekT_sys$OpenMode$Read,   &ekT_sys$OpenMode$Write,
        &ekT_sys$OpenMode$Append, &ekT_sys$OpenMode$New,
        &ekT_sys$OpenMode$Update
    };
    size_t i;

    if (!ek_val_is_ref(m)) {
        return -1;
    }
    for (i = 0; i < sizeof ids / sizeof *ids; i++) {
        if (m.as.ref->info == ids[i]) {
            return (int)i;
        }
    }
    return -1;
}

static ek_val open_call(ek_val env, ek_val *args, size_t argc)
{
    ek_val file;
    int err = EK_IO_OTHER;
    ek_obj *detail = NULL;
    int mode = argc == 2 ? mode_of(args[1]) : -1;

    (void)env;
    if (mode < 0
        || !ek_file_open_cstr(ek_gen_cstr(args[0]), mode, &file, &err,
                              &detail)) {
        return ek_gen_err_of(err, detail);
    }
    return ek_gen_ok(file);
}

/* The callback runs at most once and the handle closes on both paths;
   the close on the way out is best effort. */
static ek_val with_file_call(ek_val env, ek_val *args, size_t argc)
{
    ek_val file;
    int err = EK_IO_OTHER;
    ek_obj *detail = NULL;
    int mode;
    ek_val r;
    int cerr = 0;
    ek_obj *cdet = NULL;

    (void)env;
    mode = argc == 3 ? mode_of(args[1]) : -1;
    if (mode < 0
        || !ek_file_open_cstr(ek_gen_cstr(args[0]), mode, &file, &err,
                              &detail)) {
        return ek_gen_err_of(err, detail);
    }
    r = ek_closure_call(args[2], &file, 1);
    if (!ek_file_close(file, &cerr, &cdet) && cdet != NULL) {
        ek_release(ek_val_ref(cdet));
    }
    ek_release(file);
    return ek_gen_ok(r);
}

static ek_val exists_call(ek_val env, ek_val *args, size_t argc)
{
    struct stat st;

    (void)env;
    if (argc != 1 || stat(ek_gen_cstr(args[0]), &st) != 0) {
        return ek_bool(0);
    }
    return ek_bool(1);
}

/* Reads the open descriptor into a growing block; the storage dies
   with the caller on the error paths. */
static int read_fd_whole(int fd, char **buf, size_t *len, int *err,
                         ek_obj **detail)
{
    size_t cap = 256, used = 0;
    char *data = malloc(cap);

    if (data == NULL) {
        close(fd);
        ek_abort("out of memory");
    }
    for (;;) {
        ssize_t got;

        if (used == cap) {
            size_t ncap = cap * 2;
            char *next = realloc(data, ncap);

            if (next == NULL) {
                free(data);
                close(fd);
                ek_abort("out of memory");
            }
            data = next;
            cap = ncap;
        }
        got = read(fd, data + used, cap - used);
        if (got < 0 && errno == EINTR) {
            continue;
        }
        if (got < 0) {
            int e = errno;

            free(data);
            close(fd);
            return ek_gen_fail(err, detail, ek_gen_io_kind(e),
                               strerror(e));
        }
        if (got == 0) {
            break;
        }
        used += (size_t)got;
    }
    *buf = data;
    *len = used;
    return 1;
}

/* Reads the whole path into a growing block. */
static int read_whole(const char *path, char **buf, size_t *len, int *err,
                      ek_obj **detail)
{
    int fd = open(path, O_RDONLY);
    int ok;

    if (fd < 0) {
        return ek_gen_fail(err, detail, ek_gen_io_kind(errno),
                           strerror(errno));
    }
    ok = read_fd_whole(fd, buf, len, err, detail);
    close(fd);
    return ok;
}

/* Writes the block through the given open flags. */
static int write_whole(const char *path, int flags, const char *data,
                       size_t len, int *err, ek_obj **detail)
{
    int fd = open(path, flags, 0666);

    while (fd >= 0 && len > 0) {
        ssize_t w = write(fd, data, len);

        if (w < 0 && errno == EINTR) {
            continue;
        }
        if (w <= 0) {
            int e = errno;

            close(fd);
            return ek_gen_fail(err, detail, ek_gen_io_kind(e),
                               strerror(e));
        }
        data += w;
        len -= (size_t)w;
    }
    if (fd < 0) {
        return ek_gen_fail(err, detail, ek_gen_io_kind(errno),
                           strerror(errno));
    }
    close(fd);
    return 1;
}

/* The whole-path read in the text and the byte flavor; the text read
   keeps the UTF-8 gate. */
static ek_val read_like(ek_val *args, size_t argc, int as_text)
{
    char *data = NULL;
    size_t len = 0;
    int err = EK_IO_OTHER;
    ek_obj *detail = NULL;
    ek_obj *v;

    if (argc != 1) {
        return ek_gen_err(err, NULL);
    }
    if (!read_whole(ek_gen_cstr(args[0]), &data, &len, &err, &detail)) {
        return ek_gen_err_of(err, detail);
    }
    if (as_text && !ek_utf8_valid(data, len)) {
        free(data);
        return ek_gen_err(EK_IO_INVALID_DATA, "invalid UTF-8");
    }
    v = as_text ? ek_str_new(data, len) : ek_bytes_new(data, len);
    free(data);
    return ek_gen_ok(ek_val_ref(v));
}

static ek_val read_call(ek_val env, ek_val *args, size_t argc)
{
    (void)env;
    return read_like(args, argc, 1);
}

static ek_val read_bytes_call(ek_val env, ek_val *args, size_t argc)
{
    (void)env;
    return read_like(args, argc, 0);
}

/* The write family: Write truncates, Append adds, in the text and the
   byte flavor alike. */
static ek_val write_like(ek_val *args, size_t argc, int append,
                         const void *data, size_t len)
{
    int err = EK_IO_OTHER;
    ek_obj *detail = NULL;
    int flags = append ? O_WRONLY | O_CREAT | O_APPEND
                       : O_WRONLY | O_CREAT | O_TRUNC;

    if (argc != 2) {
        return ek_gen_err(err, NULL);
    }
    if (!write_whole(ek_gen_cstr(args[0]), flags, data, len, &err,
                     &detail)) {
        return ek_gen_err_of(err, detail);
    }
    return ek_gen_ok(ek_val_unit());
}

/* The written string's bytes, or nothing past a malformed arity. */
static void text_arg(ek_val *args, size_t argc, const char **data,
                     size_t *len)
{
    *data = argc == 2 ? ek_gen_cstr(args[1]) : "";
    *len = argc == 2 ? strlen(*data) : 0;
}

static ek_val write_call(ek_val env, ek_val *args, size_t argc)
{
    const char *data;
    size_t len;

    (void)env;
    text_arg(args, argc, &data, &len);
    return write_like(args, argc, 0, data, len);
}

static ek_val append_call(ek_val env, ek_val *args, size_t argc)
{
    const char *data;
    size_t len;

    (void)env;
    text_arg(args, argc, &data, &len);
    return write_like(args, argc, 1, data, len);
}

/* The byte payloads ride as references; an absent block writes
   nothing. */
static ek_val bytes_like(ek_val *args, size_t argc, int append)
{
    ek_obj *b = argc == 2 && ek_val_is_ref(args[1]) ? args[1].as.ref : NULL;

    return write_like(args, argc, append, b ? ek_bytes_data(b) : "",
                      b ? ek_bytes_len(b) : 0);
}

static ek_val append_bytes_call(ek_val env, ek_val *args, size_t argc)
{
    (void)env;
    return bytes_like(args, argc, 1);
}

ek_val ek_cap_fs_cwd(void)
{
    ek_val f[EK_FS_SLOTS];

    f[EK_FS_OPEN] = ek_closure_new(open_call, ek_val_unit());
    f[EK_FS_WITH_FILE] = ek_closure_new(with_file_call, ek_val_unit());
    f[EK_FS_EXISTS] = ek_closure_new(exists_call, ek_val_unit());
    f[EK_FS_READ] = ek_closure_new(read_call, ek_val_unit());
    f[EK_FS_WRITE] = ek_closure_new(write_call, ek_val_unit());
    f[EK_FS_APPEND] = ek_closure_new(append_call, ek_val_unit());
    f[EK_FS_READ_BYTES] = ek_closure_new(read_bytes_call, ek_val_unit());
    f[EK_FS_APPEND_BYTES] = ek_closure_new(append_bytes_call, ek_val_unit());
    f[EK_FS_LIST_DIR] = ek_fs_list_dir();
    f[EK_FS_MKDIR] = ek_fs_mkdir();
    f[EK_FS_MKDIR_ALL] = ek_fs_mkdir_all();
    f[EK_FS_RM] = ek_fs_rm();
    f[EK_FS_RM_DIR] = ek_fs_rm_dir();
    f[EK_FS_RENAME] = ek_fs_rename();
    f[EK_FS_STAT] = ek_fs_stat();
    f[EK_FS_REALPATH] = ek_fs_realpath();
    return ek_bundle16_new(f);
}
