#define _POSIX_C_SOURCE 200809L

#include "emit/emit.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "support/arena.h"
#include "support/buf.h"
#include "support/diag.h"

#define EMIT_MSG_CAP 480  /* collapsed compiler output kept in a message */
#define EMIT_MSG_HEAD 120 /* the first line's share of a long message */

/* Formats into arena memory; NULL on exhaustion or an encoding error. */
static char *aprintf(ek_arena *arena, const char *fmt, ...)
{
    va_list args;
    char *s;
    int n;

    va_start(args, fmt);
    n = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (n < 0) {
        return NULL;
    }
    s = ek_arena_alloc(arena, (size_t)n + 1);
    if (s == NULL) {
        return NULL;
    }
    va_start(args, fmt);
    vsnprintf(s, (size_t)n + 1, fmt, args);
    va_end(args);
    return s;
}

/* Writes the translation unit to path, overwriting; 1 on success. */
static int write_unit(const char *path, const char *text, size_t len)
{
    FILE *fp = fopen(path, "wb");

    if (fp == NULL) {
        return 0;
    }
    if (len != 0 && fwrite(text, 1, len, fp) != len) {
        fclose(fp);
        return 0;
    }
    return fclose(fp) == 0;
}

/* 1 when the runtime include directory and the archive are in place. */
static int runtime_present(const char *incdir, const char *archive)
{
    struct stat st;

    return stat(incdir, &st) == 0 && S_ISDIR(st.st_mode)
        && stat(archive, &st) == 0 && S_ISREG(st.st_mode);
}

/* Moves a cut point forward onto a scalar lead byte, so a truncated
   message never starts inside a UTF-8 sequence. */
static size_t align_forward(const char *s, size_t len, size_t at)
{
    while (at < len && ((unsigned char)s[at] & 0xC0) == 0x80) {
        at++;
    }
    return at;
}

/* Appends text with every run of whitespace collapsed to one space and
   the ends trimmed. */
static void append_collapsed(ek_buf *out, const char *text, size_t len)
{
    size_t i;
    int gap = 0;

    for (i = 0; i < len; i++) {
        if (isspace((unsigned char)text[i])) {
            gap = 1;
            continue;
        }
        if (gap != 0 && out->len != 0) {
            if (!ek_buf_append_char(out, ' ')) {
                return;
            }
        }
        gap = 0;
        if (!ek_buf_append_char(out, text[i])) {
            return;
        }
    }
}

/* The E-emit message for a failed compile: the compiler output on one
   line, or the bare fact when it printed nothing. An output too long
   for one message keeps the first line and the tail. */
static void emit_failure(ek_diag *diag, const char *out_path,
                         const char *raw, size_t len)
{
    ek_buf flat;
    const char *nl;
    size_t first_len;

    if (len == 0 || raw == NULL) {
        ek_diag_emit(diag, EK_SEV_ERROR, "E-emit", out_path, 0, 0,
                     "cc failed");
        return;
    }
    ek_buf_init(&flat);
    append_collapsed(&flat, raw, len);
    nl = memchr(raw, '\n', len);
    first_len = nl != NULL ? (size_t)(nl - raw) : len;
    if (flat.data == NULL) {
        ek_diag_emit(diag, EK_SEV_ERROR, "E-emit", out_path, 0, 0,
                     "cc failed");
    } else if (flat.len <= EMIT_MSG_CAP) {
        ek_diag_emit(diag, EK_SEV_ERROR, "E-emit", out_path, 0, 0,
                     "cc failed: %s", flat.data);
    } else {
        ek_buf head;
        size_t cut;

        ek_buf_init(&head);
        append_collapsed(&head, raw, first_len);
        if (head.data != NULL && head.len > EMIT_MSG_HEAD) {
            cut = align_forward(head.data, head.len, EMIT_MSG_HEAD);
            head.data[cut] = '\0';
            head.len = cut;
        }
        cut = flat.len - (EMIT_MSG_CAP - EMIT_MSG_HEAD - 5);
        cut = align_forward(flat.data, flat.len, cut);
        ek_diag_emit(diag, EK_SEV_ERROR, "E-emit", out_path, 0, 0,
                     "cc failed: %s ... %s",
                     head.data != NULL ? head.data : "",
                     flat.data + cut);
        ek_buf_destroy(&head);
    }
    ek_buf_destroy(&flat);
}

/* Runs the link line through the shell; returns the wait status, with
   the compiler's output appended to out. The line ends with a
   redirection so its stderr lands in the same stream. */
static int run_cc(const char *cmd, ek_buf *out)
{
    FILE *fp;
    char chunk[4096];
    size_t got;
    ek_buf line;

    ek_buf_init(&line);
    if (!ek_buf_append_str(&line, cmd)
        || !ek_buf_append_str(&line, " 2>&1")) {
        ek_buf_destroy(&line);
        return -1;
    }
    fp = popen(line.data, "r");
    ek_buf_destroy(&line);
    if (fp == NULL) {
        return -1;
    }
    while ((got = fread(chunk, 1, sizeof chunk, fp)) > 0) {
        (void)ek_buf_append_bytes(out, chunk, got);
    }
    return pclose(fp);
}

/* Writes the unit and checks the runtime tree; 0 with a diagnostic
   when the stage cannot reach the compile. */
static int prepare(ek_diag *diag, ek_arena *arena, const char *c_text,
                   const char *out_path, const char *tree_root)
{
    char *src_path, *incdir, *archive;

    if (out_path == NULL || tree_root == NULL) {
        ek_diag_emit(diag, EK_SEV_ERROR, "E-emit", out_path, 0, 0,
                     "no output path or runtime tree");
        return 0;
    }
    if (c_text == NULL) {
        ek_diag_emit(diag, EK_SEV_ERROR, "E-emit", out_path, 0, 0,
                     "no translation unit to write");
        return 0;
    }
    src_path = aprintf(arena, "%s.c", out_path);
    incdir = aprintf(arena, "%s/rt", tree_root);
    archive = aprintf(arena, "%s/build/libekort.a", tree_root);
    if (src_path == NULL || incdir == NULL || archive == NULL) {
        ek_diag_emit(diag, EK_SEV_ERROR, "E-emit", out_path, 0, 0,
                     "out of memory");
        return 0;
    }
    if (!write_unit(src_path, c_text, strlen(c_text))) {
        ek_diag_emit(diag, EK_SEV_ERROR, "E-emit", out_path, 0, 0,
                     "cannot write the translation unit to %s", src_path);
        return 0;
    }
    if (!runtime_present(incdir, archive)) {
        ek_diag_emit(diag, EK_SEV_ERROR, "E-emit", out_path, 0, 0,
                     "runtime bits are missing under %s", tree_root);
        return 0;
    }
    return 1;
}

ek_emit_result ek_emit_aot(const char *c_text, const char *out_path,
                           const char *tree_root, ek_arena *arena,
                           ek_diag *diag)
{
    ek_emit_result r;
    ek_buf raw;
    char *cmd;
    int status;

    r.ok = 0;
    if (!prepare(diag, arena, c_text, out_path, tree_root)) {
        return r;
    }
    cmd = aprintf(arena, "cc -std=c99 -I%s/rt %s.c %s/build/libekort.a"
                         " -lm -o %s",
                  tree_root, out_path, tree_root, out_path);
    if (cmd == NULL) {
        ek_diag_emit(diag, EK_SEV_ERROR, "E-emit", out_path, 0, 0,
                     "out of memory");
        return r;
    }
    ek_buf_init(&raw);
    status = run_cc(cmd, &raw);
    if (status < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        emit_failure(diag, out_path, raw.data, raw.len);
    } else {
        r.ok = 1;
    }
    ek_buf_destroy(&raw);
    return r;
}
