#define _POSIX_C_SOURCE 200809L

#include "internal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* The file handles: a table of open streams with a generation counter
   per slot. A File value packs its slot and the slot's generation into
   the capability module's handle record, so a copy taken before a
   close turns stale: every operation rechecks the generation and
   reports the closed reason instead of touching the stream. Leaks are
   permitted; without a close the operating system reclaims the stream
   at process exit. */

#define EK_FILE_SLOTS 64

static struct {
    FILE *fp;
    unsigned gen;
} ek_file_slots[EK_FILE_SLOTS];

/* The handle record: the capability module's [raw: UInt] over the
   object header, the field in the first slot after it (the layout
   contract of internal.h). Runtime-typed: the record is field-read and
   dropped, never pattern-matched. */
typedef struct {
    ek_obj obj;
    ek_val f_raw;
} ek_file_rec;

static void file_rec_drop(ek_obj *obj)
{
    ek_release(((ek_file_rec *)obj)->f_raw);
}

static const ek_typeinfo ek_typeinfo_file_rec = { EK_KIND_STRUCT,
                                                  file_rec_drop };

static ek_val file_val(size_t slot, unsigned gen)
{
    ek_file_rec *rec = ek_rt_alloc_zeroed(sizeof *rec);

    rec->obj.info = &ek_typeinfo_file_rec;
    rec->obj.rc = 1;
    rec->f_raw = ek_val_uint(((uint64_t)slot << 32) | (uint64_t)gen);
    return ek_val_ref(&rec->obj);
}

/* The failure return: the reason in *err, the detail string the two
   payload kinds carry. */
static int fail(int *err, ek_obj **detail, int kind, const char *msg)
{
    *err = kind;
    *detail = msg != NULL ? ek_str_from_cstr(msg) : NULL;
    return 0;
}

/* The errno a failed system call maps onto the closed reason union. */
static int io_kind_of(int e)
{
    switch (e) {
    case ENOENT:  return EK_IO_NOT_FOUND;
    case EEXIST:  return EK_IO_EXISTS;
    case EACCES:
    case EPERM:
    case EROFS:   return EK_IO_PERMISSION;
    case EISDIR:
    case ENOTDIR: return EK_IO_NOT_FILE;
    default:      return EK_IO_OTHER;
    }
}

/* The stream behind a live handle, or NULL with the failure written:
   anything but a handle over an open slot with the same generation is
   stale. */
static FILE *handle_fp(ek_val f, int *err, ek_obj **detail)
{
    size_t slot;
    unsigned gen;

    if (!ek_val_is_ref(f)
        || f.as.ref->info != &ek_typeinfo_file_rec
        || ((const ek_file_rec *)f.as.ref)->f_raw.tag != EK_VAL_UINT) {
        fail(err, detail, EK_IO_CLOSED, NULL);
        return NULL;
    }
    slot = (size_t)(((const ek_file_rec *)f.as.ref)->f_raw.as.u >> 32);
    gen = (unsigned)(((const ek_file_rec *)f.as.ref)->f_raw.as.u & 0xFFFFFFFFu);
    if (slot >= EK_FILE_SLOTS || ek_file_slots[slot].fp == NULL
        || ek_file_slots[slot].gen != gen) {
        fail(err, detail, EK_IO_CLOSED, NULL);
        return NULL;
    }
    return ek_file_slots[slot].fp;
}

/* The open modes in the capability module's variant order, as open(2)
   flags and the matching stream mode. */
static const struct {
    int flags;
    const char *stream;
} open_modes[] = {
    { O_RDONLY, "r" },
    { O_WRONLY | O_CREAT | O_TRUNC, "w" },
    { O_WRONLY | O_CREAT | O_APPEND, "a" },
    { O_WRONLY | O_CREAT | O_EXCL, "w" },
    { O_RDWR, "r+" },
};

int ek_file_open_cstr(const char *path, int mode, ek_val *file, int *err,
                      ek_obj **detail)
{
    int fd;
    int e;
    FILE *fp;
    struct stat st;
    size_t i;

    if (mode < 0 || mode >= (int)(sizeof open_modes / sizeof *open_modes)) {
        return fail(err, detail, EK_IO_OTHER, "unknown open mode");
    }
    fd = open(path, open_modes[mode].flags, 0666);
    if (fd < 0) {
        return fail(err, detail, io_kind_of(errno), strerror(errno));
    }
    if (fstat(fd, &st) != 0) {
        e = errno;
        close(fd);
        return fail(err, detail, io_kind_of(e), strerror(e));
    }
    if (!S_ISREG(st.st_mode)) {
        close(fd);
        return fail(err, detail, EK_IO_NOT_FILE, "not a regular file");
    }
    fp = fdopen(fd, open_modes[mode].stream);
    if (fp == NULL) {
        e = errno;
        close(fd);
        return fail(err, detail, io_kind_of(e), strerror(e));
    }
    for (i = 0; i < EK_FILE_SLOTS; i++) {
        if (ek_file_slots[i].fp == NULL) {
            ek_file_slots[i].fp = fp;
            *file = file_val(i, ek_file_slots[i].gen);
            return 1;
        }
    }
    fclose(fp);
    return fail(err, detail, EK_IO_OTHER, "too many open files");
}

/* Doubles the read buffer before the next byte lands. */
static char *grow_buf(char *buf, size_t *cap)
{
    size_t ncap = *cap * 2;
    char *next = realloc(buf, ncap);

    if (next == NULL) {
        free(buf);
        ek_abort("out of memory");
    }
    *cap = ncap;
    return next;
}

/* Reads at most n Unicode scalar values; the text gate rejects bytes
   that are not well-formed UTF-8. */
static int read_chars(FILE *fp, int64_t n, ek_obj **out, int *err,
                      ek_obj **detail)
{
    size_t cap = 32, len = 0, done = 0;
    char *buf = malloc(cap);
    int64_t got = 0;

    if (buf == NULL) {
        ek_abort("out of memory");
    }
    while (got < n) {
        int c = fgetc(fp);
        size_t seqlen;

        if (c == EOF) {
            break;
        }
        if (len == cap) {
            buf = grow_buf(buf, &cap);
        }
        buf[len++] = (char)c;
        seqlen = ek_utf8_seq_len((const unsigned char *)buf + done,
                                 len - done);
        if (seqlen > 0) {
            done = len;
            got++;
        } else if (len - done >= 4) {
            free(buf);
            return fail(err, detail, EK_IO_INVALID_DATA, "invalid UTF-8");
        }
    }
    if (done < len) {
        free(buf);
        return fail(err, detail, EK_IO_INVALID_DATA, "invalid UTF-8");
    }
    *out = ek_str_new(buf, len);
    free(buf);
    return 1;
}

/* Reads one line from the stream, without its terminating newline: a
   fresh String object, or NULL at end of input with *eof set, or NULL
   with *bad set when the bytes are not well-formed UTF-8. */
ek_obj *ek_line_read(FILE *fp, int *eof, int *bad)
{
    size_t cap = 64, len = 0;
    char *buf = malloc(cap);
    int c;
    int any = 0;

    if (buf == NULL) {
        ek_abort("out of memory");
    }
    while ((c = fgetc(fp)) != EOF) {
        any = 1;
        if (c == '\n') {
            break;
        }
        if (len == cap) {
            buf = grow_buf(buf, &cap);
        }
        buf[len++] = (char)c;
    }
    if (!any) {
        free(buf);
        *eof = 1;
        *bad = 0;
        return NULL;
    }
    *eof = 0;
    if (!ek_utf8_valid(buf, len)) {
        free(buf);
        *bad = 1;
        return NULL;
    }
    *bad = 0;
    {
        ek_obj *s = ek_str_new(buf, len);

        free(buf);
        return s;
    }
}

int ek_file_read(ek_val f, int64_t nchars, ek_obj **out, int *err,
                 ek_obj **detail)
{
    FILE *fp = handle_fp(f, err, detail);

    if (fp == NULL) {
        return 0;
    }
    return read_chars(fp, nchars, out, err, detail);
}

int ek_file_read_bytes(ek_val f, int64_t n, ek_obj **out, int *err,
                       ek_obj **detail)
{
    FILE *fp = handle_fp(f, err, detail);
    size_t want, got;
    char *buf;

    if (fp == NULL) {
        return 0;
    }
    want = n > 0 ? (size_t)n : 0;
    buf = malloc(want + 1);
    if (buf == NULL) {
        ek_abort("out of memory");
    }
    got = fread(buf, 1, want, fp);
    *out = ek_bytes_new(buf, got);
    free(buf);
    return 1;
}

int ek_file_read_line(ek_val f, ek_obj **out, int *err, ek_obj **detail)
{
    FILE *fp = handle_fp(f, err, detail);
    ek_obj *line;
    int eof = 0, bad = 0;

    if (fp == NULL) {
        return 0;
    }
    line = ek_line_read(fp, &eof, &bad);
    if (bad) {
        return fail(err, detail, EK_IO_INVALID_DATA, "invalid UTF-8");
    }
    *out = line; /* NULL at end of input, which is success */
    return 1;
}

/* The shared write path: the block lands at the current position. */
static int write_block(FILE *fp, const void *data, size_t len, int *err,
                       ek_obj **detail)
{
    if (len > 0 && fwrite(data, 1, len, fp) != len) {
        return fail(err, detail, EK_IO_OTHER, "write failed");
    }
    return 1;
}

int ek_file_write(ek_val f, ek_obj *s, int *err, ek_obj **detail)
{
    FILE *fp = handle_fp(f, err, detail);

    if (fp == NULL) {
        return 0;
    }
    return write_block(fp, ek_str_cstr(s), ek_str_len(s), err, detail);
}

int ek_file_write_bytes(ek_val f, ek_obj *b, int *err, ek_obj **detail)
{
    FILE *fp = handle_fp(f, err, detail);

    if (fp == NULL) {
        return 0;
    }
    return write_block(fp, ek_bytes_data(b), ek_bytes_len(b), err, detail);
}

int ek_file_seek(ek_val f, ek_val pos, int *err, ek_obj **detail)
{
    FILE *fp = handle_fp(f, err, detail);
    struct stat st;
    uint64_t at;

    if (fp == NULL) {
        return 0;
    }
    at = pos.tag == EK_VAL_UINT ? pos.as.u : 0;
    if (fstat(fileno(fp), &st) != 0) {
        return fail(err, detail, EK_IO_OTHER, strerror(errno));
    }
    if (at > (uint64_t)st.st_size) {
        return fail(err, detail, EK_IO_INVALID_DATA,
                    "seek past end of file");
    }
    if (fseek(fp, (long)at, SEEK_SET) != 0) {
        return fail(err, detail, EK_IO_OTHER, strerror(errno));
    }
    return 1;
}

int ek_file_flush(ek_val f, int *err, ek_obj **detail)
{
    FILE *fp = handle_fp(f, err, detail);

    if (fp == NULL) {
        return 0;
    }
    if (fflush(fp) != 0) {
        return fail(err, detail, EK_IO_OTHER, strerror(errno));
    }
    return 1;
}

int ek_file_close(ek_val f, int *err, ek_obj **detail)
{
    FILE *fp = handle_fp(f, err, detail);
    size_t slot;

    if (fp == NULL) {
        return 0;
    }
    slot = (size_t)(((const ek_file_rec *)f.as.ref)->f_raw.as.u >> 32);
    ek_file_slots[slot].fp = NULL;
    ek_file_slots[slot].gen++;
    if (fclose(fp) != 0) {
        return fail(err, detail, EK_IO_OTHER, strerror(errno));
    }
    return 1;
}
