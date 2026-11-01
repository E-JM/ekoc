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


/* The build command: compiles one Eko file into an executable and
   returns the process exit status, 0 on success and 2 on any failure.
   out is the executable path, or NULL for the input's stem in the
   current directory. tree_root is the directory holding rt/ and
   build/. */
int cli_build(const char *file, const char *out, const char *tree_root);

/* The test command: compiles one Eko file's test bindings into a
   driver binary under tree_root's build/ and runs it once per test,
   reporting one line per test. Returns the process exit status, 0
   when every test passed and 2 on any failure before or during the
   run. tree_root is the directory holding rt/ and build/. */
int cli_test(const char *file, const char *tree_root);

#endif
