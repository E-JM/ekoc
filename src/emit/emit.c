#define _POSIX_C_SOURCE 200809L

#include "emit/emit.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "support/arena.h"
#include "support/buf.h"
#include "support/diag.h"

#define EMIT_MSG_CAP 480  /* compiler output kept whole in a message */
#define EMIT_MSG_HEAD 120 /* the head's share of a long message */

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

/* The E-emit message for a failed compile: the compiler's own lines
   stand whole, never folded onto one; an output too long for one
   message keeps its head and tail lines. */
static void emit_failure(ek_diag *diag, const char *out_path,
                         const char *raw, size_t len)
{
    if (len == 0 || raw == NULL) {
        ek_diag_emit(diag, EK_SEV_ERROR, "E-emit", out_path, 0, 0,
                     "cc failed");
        return;
    }
    if (len <= EMIT_MSG_CAP) {
        ek_diag_emit(diag, EK_SEV_ERROR, "E-emit", out_path, 0, 0,
                       "cc failed:\n%.*s", (int)len, raw);
        return;
    }
    {
        size_t cut = len - (EMIT_MSG_CAP - EMIT_MSG_HEAD - 20);

        cut = align_forward(raw, len, cut);
        while (cut < len && raw[cut] != '\n') {
            cut++;
        }
        ek_diag_emit(diag, EK_SEV_ERROR, "E-emit", out_path, 0, 0,
                       "cc failed (first lines cut):\n%.*s",
                       (int)(len - cut), raw + cut);
    }
}

/* Drops the temporary product a failed link may have left behind. */
static void remove_tmp(ek_arena *arena, const char *out_path)
{
    char *tmp = aprintf(arena, "%s.tmp", out_path);

    if (tmp) {
        remove(tmp);
    }
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
                           const char *tree_root, const char *extra,
                           ek_arena *arena, ek_diag *diag)
{
    ek_emit_result r;
    ek_buf raw;
    char *cmd;
    int status;

    r.ok = 0;
    if (!prepare(diag, arena, c_text, out_path, tree_root)) {
        return r;
    }
    /* the link lands on a temporary name and replaces the product by
       rename, so a failed compile leaves the previous binary whole;
       the extra arguments carry the host's libraries and shims */
    cmd = aprintf(arena, "cc -std=c99 -pthread -I%s/rt %s.c %s/build/libekort.a"
                         " -lm %s -o %s.tmp 2>&1 && mv %s.tmp %s",
                  tree_root, out_path, tree_root,
                  extra != NULL ? extra : "", out_path, out_path,
                  out_path);
    if (cmd == NULL) {
        ek_diag_emit(diag, EK_SEV_ERROR, "E-emit", out_path, 0, 0,
                     "out of memory");
        return r;
    }
    ek_buf_init(&raw);
    status = run_cc(cmd, &raw);
    if (status < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        remove_tmp(arena, out_path);
        emit_failure(diag, out_path, raw.data, raw.len);
    } else {
        r.ok = 1;
    }
    ek_buf_destroy(&raw);
    return r;
}
