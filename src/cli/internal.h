#ifndef EK_CLI_INTERNAL_H
#define EK_CLI_INTERNAL_H

#include <stddef.h>

#include "check/check.h"
#include "lower/lower.h"
#include "resolve/resolve.h"
#include "support/arena.h"
#include "support/buf.h"
#include "support/diag.h"

/* Shared declarations of the ekoc program. The CLI is a program, not a
   pipeline stage: it has no public header. */

/* Diagnostics drained from the pipeline stages, in emission order.
   Messages live in the diag buffer's own storage and stay valid until
   it is destroyed, so the log holds shallow copies. */
typedef struct {
    ek_diagnostic *items;
    size_t count;
    size_t cap;
    ek_arena *arena;
} cli_diag_log;

/* Appends one drained buffer. A dropped run means an exhausted arena,
   which the stages report on their own. */
void cli_log_drain(cli_diag_log *log, ek_diag *diag);

/* Prints every diagnostic as one report line. */
void cli_log_print(const cli_diag_log *log);

/* The number of error-severity diagnostics drained so far. */
size_t cli_log_errors(const cli_diag_log *log);

/* Reads the whole file; 1 on success. */
int cli_read_source(ek_buf *buf, const char *path);

/* Everything before the last slash; "." when the path has none. */
const char *cli_dir_of(ek_arena *arena, const char *path);

/* The input's stem: the last component without its extension. */
const char *cli_stem_of(ek_arena *arena, const char *path);

/* The front of the pipeline, source bytes to the checked AST, with
   each stage's diagnostics drained into log. res and chk receive the
   resolution of the compilation root with its closure and the check
   over it. Returns 0 when an early stage lost its output to an
   exhausted arena. */
int cli_resolve(cli_diag_log *log, ek_arena *arena, ek_diag *diag,
                const char *file, const char *dir,
                const char *bytes, size_t len,
                ek_resolve_result *res, ek_check_result *chk);

/* True when the root module declares a `main` function binding; the
   host commands own its presence, a test root is exempt. */
int cli_has_main(const ek_resolve_result *res);

/* One module surface's test bindings in declaration order; names may
   be NULL for a counting pass. A NULL surface holds no tests. */
size_t cli_test_names(const ek_surface *s, const char **names);

/* The build command: compiles one Eko file into an executable and
   returns the process exit status, 0 on success and 2 on any failure.
   out is the executable path, or NULL for the input's stem in the
   current directory. tree_root is the directory holding rt/ and
   build/. */
int cli_build(const char *file, const char *out, const char *tree_root);

/* The check command: the pipeline through the lowerer over one Eko
   file, every diagnostic reported and nothing compiled. A root that
   declares test bindings takes the test root's rules -- no `main`
   requirement; a program root must declare `main`, exactly as build
   enforces it. Returns the process exit status, 0 when the only
   diagnostics are warnings. */
int cli_check(const char *file);

/* The test command: collects the test bindings of every module in the
   compilation unit -- the entry file and its transitive imports -- and
   lowers and runs one driver binary per module that declares tests,
   one run per test under that module's report header. Returns the
   process exit status, 0 when every test passed and 2 on any failure
   before or during the run; a unit with no test binding anywhere is a
   failure. tree_root is the directory holding rt/ and build/. */
int cli_test(const char *file, const char *tree_root);

#endif
