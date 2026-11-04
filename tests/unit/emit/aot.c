#define _POSIX_C_SOURCE 200809L

#include "harness.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "emit/emit.h"
#include "support/arena.h"
#include "support/diag.h"

/* The emit gate: a hand-written unit goes through ek_emit_aot against
   the repository's own runtime tree (the working directory is the
   repository root), and the linked binary runs in a forked child. */

static const char *unit_exit_0 =
    "#include \"ekort.h\"\n"
    "\n"
    "int main(void)\n"
    "{\n"
    "    return 0;\n"
    "}\n";

static const char *unit_exit_3 =
    "#include \"ekort.h\"\n"
    "\n"
    "int main(void)\n"
    "{\n"
    "    return 3;\n"
    "}\n";

static const char *unit_broken =
    "#include \"ekort.h\"\n"
    "\n"
    "int main(void)\n"
    "{\n"
    "    return 0\n"
    "}\n";

/* Runs the binary in a child; returns the wait status. */
static int run_binary(const char *path)
{
    pid_t pid = fork();
    int status = -1;

    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        execl(path, path, (char *)NULL);
        _exit(127);
    }
    waitpid(pid, &status, 0);
    return status;
}

/* Drains diag and returns the first E-emit diagnostic, or NULL. */
static const ek_diagnostic *emit_error(ek_diag *diag)
{
    const ek_diagnostic *ds;
    size_t i, n = 0;

    ds = ek_diag_drain(diag, &n);
    for (i = 0; i < n; i++) {
        if (ds[i].code != NULL && strcmp(ds[i].code, "E-emit") == 0) {
            return &ds[i];
        }
    }
    return NULL;
}

static void success_path(void)
{
    ek_arena *arena = ek_arena_create();
    ek_diag *diag = ek_diag_create();
    ek_emit_result r;
    int status;

    if (arena == NULL || diag == NULL) {
        ek_diag_destroy(diag);
        ek_arena_destroy(arena);
        return;
    }
    r = ek_emit_aot(unit_exit_0, "build/emit_aot_ok", ".", arena, diag);
    EK_CHECK_U64(r.ok, 1);
    EK_CHECK_U64(ek_diag_count(diag, EK_SEV_ERROR), 0);
    EK_CHECK(access("build/emit_aot_ok.c", R_OK) == 0);
    EK_CHECK(access("build/emit_aot_ok", X_OK) == 0);
    status = run_binary("build/emit_aot_ok");
    EK_CHECK(status >= 0 && WIFEXITED(status));
    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    /* A second emit over the same path replaces the unit and the
       executable wholesale. */
    r = ek_emit_aot(unit_exit_3, "build/emit_aot_ok", ".", arena, diag);
    EK_CHECK_U64(r.ok, 1);
    status = run_binary("build/emit_aot_ok");
    EK_CHECK(status >= 0 && WIFEXITED(status));
    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 3);
    }
    ek_diag_destroy(diag);
    ek_arena_destroy(arena);
}

static void failure_path(void)
{
    ek_arena *arena = ek_arena_create();
    ek_diag *diag = ek_diag_create();
    ek_emit_result r;
    const ek_diagnostic *fail;

    if (arena == NULL || diag == NULL) {
        ek_diag_destroy(diag);
        ek_arena_destroy(arena);
        return;
    }
    (void)remove("build/emit_aot_bad");
    r = ek_emit_aot(unit_broken, "build/emit_aot_bad", ".", arena, diag);
    EK_CHECK_U64(r.ok, 0);
    fail = emit_error(diag);
    EK_CHECK(fail != NULL);
    if (fail != NULL) {
        /* The message carries the compiler's own lines, whole. */
        EK_CHECK(strstr(fail->message, "error") != NULL);
        EK_CHECK(strchr(fail->message, '\n') != NULL);
    }
    /* a failed link leaves no product and no temporary behind */
    EK_CHECK(access("build/emit_aot_bad", X_OK) != 0);
    EK_CHECK(access("build/emit_aot_bad.tmp", X_OK) != 0);
    ek_diag_destroy(diag);
    ek_arena_destroy(arena);
}

int main(void)
{
    if (mkdir("build", 0777) != 0 && errno != EEXIST) {
        fprintf(stderr, "emit/aot: cannot create build/\n");
        return 1;
    }
    success_path();
    failure_path();
    return ek_test_report("emit/aot");
}
