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

/* The test command: every module of the compilation unit declaring at
   least one test binding is a test root. The roots run one by one,
   each lowered with its own module as the test root into its own
   driver binary, so same-named tests of different modules never share
   a dispatch. Panic capture is a process boundary: a failing test
   panics in the child, and the driver reads the panic block from the
   child's standard error and reports it. The child's standard output
   passes through, so a test that exercises a derived output channel
   behaves as it would under its own runner. */

/* One test root: a closure module, its test names in declaration
   order, and the driver binary lowered from it. */
typedef struct {
    const ek_surface *surface; /* the module's surface in the closure */
    size_t index;              /* the surface's place in the closure */
    const char **names;
    size_t ntests;
    const char *bin;
} test_root;

/* The driver binary's path: tree_root's build/ holding the compiler's
   own artifacts, named after the entry and the test root so reruns
   overwrite and the roots of one unit never share a path. */
static const char *binary_path(ek_arena *arena, const char *tree_root,
                               const char *entry_stem, const char *mod_stem)
{
    char *path;
    size_t n;

    n = snprintf(NULL, 0, "%s/build/ekoc-test-%s-%s", tree_root,
                 entry_stem, mod_stem);
    if (n == (size_t)-1) {
        return NULL;
    }
    path = ek_arena_alloc(arena, n + 1);
    if (path == NULL) {
        return NULL;
    }
    snprintf(path, n + 1, "%s/build/ekoc-test-%s-%s", tree_root,
             entry_stem, mod_stem);
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

/* The unit's test roots in closure order -- the entry module first,
   then the imported modules in the order resolve discovered them.
   The order across roots is this implementation's choice; within a
   root the declaration order holds. Returns 0 when an allocation
   failed. */
static int collect_roots(ek_arena *arena, const ek_resolve_result *res,
                         test_root **roots, size_t *nroots, size_t *ntotal)
{
    test_root *found;
    size_t count, total, i, k;

    *roots = NULL;
    *nroots = 0;
    *ntotal = 0;
    if (res == NULL || res->module_count == 0) {
        return 1;
    }
    count = 0;
    for (i = 0; i < res->module_count; i++) {
        if (cli_test_names(&res->modules[i], NULL) != 0) {
            count++;
        }
    }
    if (count == 0) {
        return 1;
    }
    found = ek_arena_alloc(arena, count * sizeof *found);
    if (found == NULL) {
        return 0;
    }
    k = 0;
    total = 0;
    for (i = 0; i < res->module_count; i++) {
        const ek_surface *s = &res->modules[i];
        size_t n = cli_test_names(s, NULL);

        if (n == 0) {
            continue;
        }
        found[k].names = ek_arena_alloc(arena, n * sizeof *found[k].names);
        if (found[k].names == NULL) {
            return 0;
        }
        cli_test_names(s, found[k].names);
        found[k].surface = s;
        found[k].index = i;
        found[k].ntests = n;
        found[k].bin = NULL;
        total += n;
        k++;
    }
    *roots = found;
    *nroots = k;
    *ntotal = total;
    return 1;
}

/* The closure with the root's module moved to the front: the lowerer
   takes a test unit's bindings from the first module, and the command
   lowers one root at a time. The surfaces are borrowed, so the
   rotation is a shallow copy. Returns 0 when the copy lost to an
   exhausted arena. */
static int rooted_at(const ek_resolve_result *res, size_t at,
                     ek_arena *arena, ek_resolve_result *out)
{
    ek_surface *mods;

    mods = ek_arena_alloc(arena, res->module_count * sizeof *mods);
    if (mods == NULL) {
        return 0;
    }
    memcpy(mods, res->modules, res->module_count * sizeof *mods);
    mods[0] = res->modules[at];
    mods[at] = res->modules[0];
    *out = *res;
    out->modules = mods;
    return 1;
}

/* Lowers and emits one driver binary per test root; each holds only
   its own module's tests. Returns 0 when an allocation failed. */
static int build_drivers(const ek_resolve_result *res, ek_check_result chk,
                         const char *entry_stem, const char *tree_root,
                         test_root *roots, size_t nroots, ek_arena *arena,
                         ek_diag *diag, cli_diag_log *log)
{
    size_t r;

    for (r = 0; r < nroots; r++) {
        ek_resolve_result rot;
        ek_lower_result low;

        if (!rooted_at(res, roots[r].index, arena, &rot)) {
            return 0;
        }
        low = ek_lower_with(rot, chk, EK_LOWER_TESTS, arena, diag);
        cli_log_drain(log, diag);
        roots[r].bin = binary_path(arena, tree_root, entry_stem,
                                   roots[r].surface->module);
        if (roots[r].bin == NULL) {
            return 0;
        }
        ek_emit_aot(low.text, roots[r].bin, tree_root, NULL, arena, diag);
        cli_log_drain(log, diag);
    }
    return 1;
}

/* Runs every root's tests in collection order: one module header, then
   one report line per test. Returns the passed count. */
static size_t run_roots(const test_root *roots, size_t nroots)
{
    size_t r, i, passed = 0;

    for (r = 0; r < nroots; r++) {
        printf("module %s\n", roots[r].surface->module);
        for (i = 0; i < roots[r].ntests; i++) {
            ek_buf err;
            int waited;

            ek_buf_init(&err);
            waited = run_test(roots[r].bin, roots[r].names[i], &err);
            passed += (size_t)report_test(roots[r].names[i], waited, &err);
            ek_buf_destroy(&err);
        }
    }
    return passed;
}

/* Frees the compilation's wholesale memory. */
static void finish(ek_buf *source, ek_diag *diag, ek_arena *arena)
{
    ek_buf_destroy(source);
    ek_diag_destroy(diag);
    ek_arena_destroy(arena);
}

/* Reads the entry file and resolves the unit. Returns 0 after the
   failure's report printed. */
static int test_resolve(cli_diag_log *log, ek_arena *arena, ek_diag *diag,
                        const char *file, ek_buf *source,
                        ek_resolve_result *res, ek_check_result *chk)
{
    if (!cli_read_source(source, file)) {
        ek_diag_emit(diag, EK_SEV_ERROR, "E-io", file, 0, 0,
                     "cannot read file: %s", strerror(errno));
        cli_log_drain(log, diag);
        cli_log_print(log);
        return 0;
    }
    cli_resolve(log, arena, diag, file, cli_dir_of(arena, file),
                source->data != NULL ? source->data : "", source->len,
                res, chk);
    if (cli_log_errors(log) != 0) {
        cli_log_print(log);
        return 0;
    }
    return 1;
}

/* The unit's roots and their test count. Returns 0 after the refusal
   of a unit with no test binding anywhere printed. */
static int test_roots(cli_diag_log *log, ek_arena *arena, const char *file,
                      const ek_resolve_result *res, test_root **roots,
                      size_t *nroots, size_t *ntests)
{
    if (!collect_roots(arena, res, roots, nroots, ntests)) {
        return 0;
    }
    if (*ntests == 0) {
        cli_log_print(log);
        fprintf(stderr, "ekoc: %s declares no test bindings\n", file);
        return 0;
    }
    return 1;
}

int cli_test(const char *file, const char *tree_root)
{
    ek_arena *arena = ek_arena_create();
    ek_diag *diag = ek_diag_create();
    cli_diag_log log;
    ek_buf source;
    ek_resolve_result res;
    ek_check_result chk;
    test_root *roots;
    const char *entry_stem;
    size_t nroots, ntests, passed;
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
    if (!test_resolve(&log, arena, diag, file, &source, &res, &chk)) {
        finish(&source, diag, arena);
        return 2;
    }
    if (!test_roots(&log, arena, file, &res, &roots, &nroots, &ntests)) {
        finish(&source, diag, arena);
        return 2;
    }
    entry_stem = cli_stem_of(arena, file);
    if (entry_stem == NULL
        || !build_drivers(&res, chk, entry_stem, tree_root, roots, nroots,
                          arena, diag, &log)) {
        finish(&source, diag, arena);
        return 2;
    }
    /* the report always prints: warnings surface also over a clean
       run, before the per-test lines */
    cli_log_print(&log);
    if (cli_log_errors(&log) != 0) {
        finish(&source, diag, arena);
        return 2;
    }
    passed = run_roots(roots, nroots);
    printf("%lu/%lu passed\n", (unsigned long)passed, (unsigned long)ntests);
    status = passed == ntests ? 0 : 1;
    finish(&source, diag, arena);
    return status;
}
