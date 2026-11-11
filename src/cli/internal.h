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
int cli_build(const char *file, const char *out, const char *tree_root,
               const char *extra);

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

/* tools.c: the Clause 11.3 tooling commands over the embedded
   sources. */
int cli_where(const char *root, const char *stem, const char *sub);
int cli_doc(const char *root, int src, const char *name);

/* tools.c: the distribution path of a shipped source by stem; 0 when
   the stem names no shipped module. buf receives the path, truncated
   to cap bytes. */
int cli_shipped_path(const char *root, const char *stem, char *buf,
                     size_t cap);

/* at.c: the position query over the resolver's reference table and
   the check's rows: the signature and the world bit of what
   <line>:<col> denotes in one Eko file. root is the compiler's own
   tree, for the shipped sources' paths. Returns 0 when the position
   named something, 2 when it did not or the pipeline failed. */
int cli_at(const char *root, const char *file, int line, int col);

/* at_report.c: the answer side of the query, over the tables one run
   built. The lookups read the resolver's and check's results; the
   printers write the answer. All of them borrow their arguments and
   the arena, and none of them frees. */
const ek_ck_row *at_row_of(const ek_check_result *chk, const ek_node *n);
const char *at_bit_name(ek_bit b);
const ek_symbol *at_member_def(const ek_resolve_result *res,
                               const ek_check_result *chk,
                               const ek_ref *ref,
                               const ek_surface **out_surf,
                               const ek_symbol **ts);
const ek_surface *at_surf_of(const ek_resolve_result *res,
                             const ek_symbol *sym);
void at_print_sym(ek_arena *arena, const char *root,
                  const ek_surface *surf, const ek_symbol *sym,
                  const ek_check_result *chk, ek_bit occ);

#endif
