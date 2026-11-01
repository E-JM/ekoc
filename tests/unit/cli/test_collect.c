#define _POSIX_C_SOURCE 200809L

#include "harness.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "cli/internal.h"

/* The test collection gate: the pipeline front over two-module
   fixtures written to build/ under the repository root, the closure's
   module surfaces inspected through cli_test_names -- per-module
   counts, declaration order, same-named tests on different roots, and
   the unit without any test binding. */

static const char *fx_lab =
    "three: () -> Int = 3\n"
    "\n"
    "beta: test =\n"
    "    assert_eq(three(), 3)\n"
    "\n"
    "alpha: test =\n"
    "    assert(1 < 2)\n";

static const char *fx_use =
    "import coll_lab\n"
    "\n"
    "gamma: test =\n"
    "    assert_eq(coll_lab.three(), 3)\n";

static const char *fx_ca =
    "one: () -> Int = 1\n"
    "\n"
    "twin: test =\n"
    "    assert_eq(one(), 1)\n";

static const char *fx_cb =
    "two: () -> Int = 2\n"
    "\n"
    "twin: test =\n"
    "    assert_eq(two(), 2)\n";

static const char *fx_pair =
    "import coll_ca\n"
    "import coll_cb\n"
    "\n"
    "glue: test =\n"
    "    assert_eq(coll_ca.one() + coll_cb.two(), 3)\n";

static const char *fx_mute =
    "four: () -> Int = 4\n";

static const char *fx_quiet =
    "import coll_mute\n"
    "\n"
    "main: () -> _ =\n"
    "    t = coll_mute.four()\n"
    "    assert_eq(t, 4)\n"
    "    _\n";

static int write_file(const char *path, const char *text)
{
    FILE *fp = fopen(path, "wb");
    size_t len = strlen(text);

    if (fp == NULL) {
        return 0;
    }
    if (fwrite(text, 1, len, fp) != len) {
        fclose(fp);
        return 0;
    }
    return fclose(fp) == 0;
}

/* The pipeline front over one fixture file; nonzero when the unit
   resolves without errors. */
static int collect(const char *file, ek_arena *arena, ek_resolve_result *res)
{
    cli_diag_log log;
    ek_diag *diag = ek_diag_create();
    ek_buf source;
    ek_check_result chk;
    int ok;

    memset(&log, 0, sizeof log);
    memset(res, 0, sizeof *res);
    memset(&chk, 0, sizeof chk);
    ek_buf_init(&source);
    log.arena = arena;
    ok = cli_read_source(&source, file)
        && cli_resolve(&log, arena, diag, file, cli_dir_of(arena, file),
                       source.data != NULL ? source.data : "", source.len,
                       res, &chk)
        && cli_log_errors(&log) == 0;
    if (!ok) {
        cli_log_print(&log);
    }
    ek_buf_destroy(&source);
    ek_diag_destroy(diag);
    return ok;
}

/* The test count over the whole closure. */
static size_t total_tests(const ek_resolve_result *res)
{
    size_t i, n = 0;

    for (i = 0; i < res->module_count; i++) {
        n += cli_test_names(&res->modules[i], NULL);
    }
    return n;
}

/* The closure surface of a module stem, NULL when absent. */
static const ek_surface *surface_of(const ek_resolve_result *res,
                                    const char *stem)
{
    size_t i;

    for (i = 0; i < res->module_count; i++) {
        if (strcmp(res->modules[i].module, stem) == 0) {
            return &res->modules[i];
        }
    }
    return NULL;
}

/* The entry and the imported module each contribute their tests: two
   roots, three tests, the imported names in declaration order. The
   closure also holds the embedded companion surfaces, which hold no
   tests. */
static void multi_module(void)
{
    const char *names[4];
    ek_arena *arena = ek_arena_create();
    ek_resolve_result res;
    const ek_surface *use, *lab;

    EK_CHECK(write_file("build/coll_lab.eko", fx_lab));
    EK_CHECK(write_file("build/coll_use.eko", fx_use));
    EK_CHECK(collect("build/coll_use.eko", arena, &res));
    EK_CHECK(res.module_count >= 2);
    use = surface_of(&res, "coll_use");
    lab = surface_of(&res, "coll_lab");
    EK_CHECK(use != NULL && lab != NULL);
    EK_CHECK(use == &res.modules[0]);
    EK_CHECK(lab != use);
    EK_CHECK_U64(cli_test_names(use, NULL), 1);
    EK_CHECK_U64(cli_test_names(lab, NULL), 2);
    EK_CHECK_U64(cli_test_names(lab, names), 2);
    EK_CHECK_STR(names[0], "beta");
    EK_CHECK_STR(names[1], "alpha");
    EK_CHECK_U64(cli_test_names(use, names), 1);
    EK_CHECK_STR(names[0], "gamma");
    EK_CHECK_U64(total_tests(&res), 3);
    ek_arena_destroy(arena);
}

/* Two modules declaring one same-named test: both are collected, one
   per root, and neither shadows the other. */
static void same_name_two_roots(void)
{
    const char *names[4];
    ek_arena *arena = ek_arena_create();
    ek_resolve_result res;
    const ek_surface *ca, *cb;

    EK_CHECK(write_file("build/coll_ca.eko", fx_ca));
    EK_CHECK(write_file("build/coll_cb.eko", fx_cb));
    EK_CHECK(write_file("build/coll_pair.eko", fx_pair));
    EK_CHECK(collect("build/coll_pair.eko", arena, &res));
    ca = surface_of(&res, "coll_ca");
    cb = surface_of(&res, "coll_cb");
    EK_CHECK(ca != NULL && cb != NULL);
    EK_CHECK_U64(cli_test_names(ca, names), 1);
    EK_CHECK_STR(names[0], "twin");
    EK_CHECK_U64(cli_test_names(cb, names), 1);
    EK_CHECK_STR(names[0], "twin");
    EK_CHECK_U64(total_tests(&res), 3);
    ek_arena_destroy(arena);
}

/* No test binding anywhere in the unit: the total is zero, and a
   missing surface holds no tests either. */
static void no_tests_anywhere(void)
{
    ek_arena *arena = ek_arena_create();
    ek_resolve_result res;

    EK_CHECK(write_file("build/coll_mute.eko", fx_mute));
    EK_CHECK(write_file("build/coll_quiet.eko", fx_quiet));
    EK_CHECK(collect("build/coll_quiet.eko", arena, &res));
    EK_CHECK(surface_of(&res, "coll_mute") != NULL);
    EK_CHECK_U64(total_tests(&res), 0);
    EK_CHECK_U64(cli_test_names(NULL, NULL), 0);
    ek_arena_destroy(arena);
}

int main(void)
{
    if (mkdir("build", 0777) != 0 && errno != EEXIST) {
        fprintf(stderr, "cli/collect: cannot create build/\n");
        return 1;
    }
    multi_module();
    same_name_two_roots();
    no_tests_anywhere();
    return ek_test_report("cli/collect");
}
