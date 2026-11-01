#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "cli/internal.h"

#include "emit/emit.h"
#include "lower/lower.h"
#include "resolve/resolve.h"
#include "support/arena.h"
#include "support/buf.h"
#include "support/diag.h"

/* The test command: one Eko file's test bindings into a driver binary,
   then one child process per test. Panic capture is a process
   boundary: a failing test panics in the child, and the driver reads
   the panic block from the child's standard error and reports it. The
   child's standard output passes through, so a test that exercises a
   derived output channel behaves as it would under its own runner. */

/* The driver binary's path: tree_root's build/ holding the compiler's
   own artifacts, named after the input so reruns overwrite. */
static const char *binary_path(ek_arena *arena, const char *tree_root,
                               const char *stem)
{
    char *path;
    size_t n;

    n = snprintf(NULL, 0, "%s/build/ekoc-test-%s", tree_root, stem);
    if (n == (size_t)-1) {
        return NULL;
    }
    path = ek_arena_alloc(arena, n + 1);
    if (path == NULL) {
        return NULL;
    }
    snprintf(path, n + 1, "%s/build/ekoc-test-%s", tree_root, stem);
    return path;
}

/* Runs the binary for one test: the child's standard output passes
   through and its standard error lands in err. Returns the raw wait
   status, or -1 when the child could not be started. */
static int run_test(const char *bin, const char *name, ek_buf *err)
{
    int fds[2];
    pid_t pid;
    ssize_t got;
    char chunk[4096];
    int status;

    if (pipe(fds) != 0) {
        return -1;
    }
    pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return -1;
    }
    if (pid == 0) {
        dup2(fds[1], STDERR_FILENO);
        close(fds[0]);
        close(fds[1]);
        execl(bin, bin, name, (char *)NULL);
        _exit(127);
    }
    close(fds[1]);
    while ((got = read(fds[0], chunk, sizeof chunk)) > 0) {
        ek_buf_append_bytes(err, chunk, (size_t)got);
    }
    close(fds[0]);
    if (waitpid(pid, &status, 0) != pid) {
        return -1;
    }
    return status;
}

/* The panic block's first line: 1 with the code and the message
   filled when the buffer opens with the panic header. The message
   keeps to that line; a program-supplied message may span more, and
   the report quotes only the first. */
static int panic_head(const ek_buf *err, const char **code, size_t *ncode,
                      const char **msg, size_t *nmsg)
{
    const char *data = err->data != NULL ? err->data : "";
    size_t len = err->len;
    size_t end = 0;
    size_t i = 7;

    while (end < len && data[end] != '\n') {
        end++;
    }
    if (strncmp(data, "panic [", 7) != 0) {
        return 0;
    }
    while (i + 3 <= end
           && !(data[i] == ']' && data[i + 1] == ':' && data[i + 2] == ' ')) {
        i++;
    }
    if (i + 3 > end) {
        return 0;
    }
    *code = data + 7;
    *ncode = i - 7;
    *msg = data + i + 3;
    *nmsg = end - i - 3;
    return 1;
}

/* One report line per test, read off the child's exit and the panic
   block of its standard error. Returns 1 when the test passed. */
static int report_test(const char *name, int status, const ek_buf *err)
{
    const char *code, *msg;
    size_t ncode, nmsg;

    if (status < 0) {
        printf("FAIL %s: could not run\n", name);
        return 0;
    }
    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);

        if (exit_code == 0) {
            printf("ok %s\n", name);
            return 1;
        }
        if (exit_code == 1 && panic_head(err, &code, &ncode, &msg, &nmsg)) {
            printf("FAIL %s: [%.*s] %.*s\n", name, (int)ncode, code,
                   (int)nmsg, msg);
        } else {
            printf("FAIL %s: exit %d\n", name, exit_code);
        }
        return 0;
    }
    if (WIFSIGNALED(status)) {
        printf("FAIL %s: crashed (signal %d)\n", name, WTERMSIG(status));
        return 0;
    }
    printf("FAIL %s: stopped\n", name);
    return 0;
}

/* Frees the compilation's wholesale memory. */
static void finish(ek_buf *source, ek_diag *diag, ek_arena *arena)
{
    ek_buf_destroy(source);
    ek_diag_destroy(diag);
    ek_arena_destroy(arena);
}

int cli_test(const char *file, const char *tree_root)
{
    ek_arena *arena = ek_arena_create();
    ek_diag *diag = ek_diag_create();
    cli_diag_log log;
    ek_buf source;
    ek_resolve_result res;
    ek_check_result chk;
    ek_lower_result low;
    const char **names;
    const char *bin;
    size_t ntests, i, passed = 0;
    int status;

    memset(&log, 0, sizeof log);
    memset(&res, 0, sizeof res);
    memset(&chk, 0, sizeof chk);
    ek_buf_init(&source);
    if (arena == NULL || diag == NULL) {
        fprintf(stderr, "ekoc: out of memory\n");
        return 2;
    }
    log.arena = arena;
    if (!cli_read_source(&source, file)) {
        ek_diag_emit(diag, EK_SEV_ERROR, "E-io", file, 0, 0,
                     "cannot read file: %s", strerror(errno));
        cli_log_drain(&log, diag);
        cli_log_print(&log);
        finish(&source, diag, arena);
        return 2;
    }
    cli_resolve(&log, arena, diag, file, cli_dir_of(arena, file),
                source.data != NULL ? source.data : "", source.len,
                &res, &chk);
    if (cli_log_errors(&log) != 0) {
        cli_log_print(&log);
        finish(&source, diag, arena);
        return 2;
    }
    ntests = cli_test_names(&res, NULL);
    if (ntests == 0) {
        cli_log_print(&log);
        fprintf(stderr, "ekoc: %s declares no test bindings\n", file);
        finish(&source, diag, arena);
        return 2;
    }
    names = ek_arena_alloc(arena, ntests * sizeof *names);
    if (names == NULL) {
        finish(&source, diag, arena);
        return 2;
    }
    cli_test_names(&res, names);
    low = ek_lower_with(res, chk, EK_LOWER_TESTS, arena, diag);
    cli_log_drain(&log, diag);
    bin = cli_stem_of(arena, file) != NULL
              ? binary_path(arena, tree_root, cli_stem_of(arena, file))
              : NULL;
    if (bin == NULL) {
        finish(&source, diag, arena);
        return 2;
    }
    ek_emit_aot(low.text, bin, tree_root, arena, diag);
    cli_log_drain(&log, diag);
    /* the report always prints: warnings surface also over a clean
       run, before the per-test lines */
    cli_log_print(&log);
    if (cli_log_errors(&log) != 0) {
        finish(&source, diag, arena);
        return 2;
    }
    for (i = 0; i < ntests; i++) {
        ek_buf err;
        int waited;

        ek_buf_init(&err);
        waited = run_test(bin, names[i], &err);
        passed += (size_t)report_test(names[i], waited, &err);
        ek_buf_destroy(&err);
    }
    printf("%lu/%lu passed\n", (unsigned long)passed, (unsigned long)ntests);
    status = passed == ntests ? 0 : 1;
    finish(&source, diag, arena);
    return status;
}
