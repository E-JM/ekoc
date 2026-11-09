#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "cli/internal.h"

#include "lower/lower.h"
#include "support/arena.h"
#include "support/buf.h"
#include "support/diag.h"

/* The check command: one Eko file through the pipeline and the lowerer,
   with every diagnostic reported and nothing compiled. The report
   always prints, so warnings surface over a clean check; the status
   separates them from errors. A root that declares test bindings takes
   the test root's rules -- no `main` requirement (Clause 7.1.3); a
   program root must declare `main`, exactly as build enforces it. */

/* Frees the check's wholesale memory. */
static void finish(ek_buf *source, ek_diag *diag, ek_arena *arena)
{
    ek_buf_destroy(source);
    ek_diag_destroy(diag);
    ek_arena_destroy(arena);
}

int cli_check(const char *file)
{
    ek_arena *arena = ek_arena_create();
    ek_diag *diag = ek_diag_create();
    cli_diag_log log;
    ek_buf source;
    ek_resolve_result res;
    ek_check_result chk;
    const char *bytes;
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
    bytes = source.data != NULL ? source.data : "";
    cli_resolve(&log, arena, diag, file, cli_dir_of(arena, file),
                bytes, source.len, &res, &chk);
    /* A front stage that reported errors stops here: the lowerer
       would read a broken intermediate and only cascade. */
    if (cli_log_errors(&log) == 0) {
    /* the entry module alone decides the check's root kind: its own
       test bindings lift the `main` requirement */
    if (res.module_count != 0
        && cli_test_names(&res.modules[0], NULL) != 0) {
        ek_lower_with(res, chk, EK_LOWER_TESTS, arena, diag);
    } else {
        if (!cli_has_main(&res)) {
            ek_diag_emit(diag, EK_SEV_ERROR, "E-entry-point", file, 0, 0,
                         "the entry module declares no `main` "
                         "(Clause 7.1.3)");
            cli_log_drain(&log, diag);
        }
        ek_lower_with(res, chk, EK_LOWER_PROGRAM, arena, diag);
    }
    }
    cli_log_drain(&log, diag);
    cli_log_print(&log);
    status = cli_log_errors(&log) != 0 ? 2 : 0;
    finish(&source, diag, arena);
    return status;
}
