#include "common.h"

#include <sys/stat.h>

/* Anchored constructors: same-named constructors of different types
   coexist; an expected type anchors a value reference and the
   scrutinee type anchors a pattern. A reference with no anchor is
   legal only when exactly one constructor of its name is visible and
   that constructor is non-generic. */

#define CTOR_DIR "build/tmp-check-ctors"
#define CONF_DIR "tests/conformance"

/* A module fixture for the cross-module application cases; the
   conformance directory supplies geolib. */
static int put_module(const char *name, const char *content)
{
    char path[512];
    FILE *fp;
    int ok;

    mkdir("build", 0755);
    mkdir(CTOR_DIR, 0755);
    snprintf(path, sizeof path, "%s/%s", CTOR_DIR, name);
    fp = fopen(path, "wb");
    if (!fp) {
        return 0;
    }
    ok = fwrite(content, 1, strlen(content), fp) == strlen(content);
    fclose(fp);
    return ok;
}

/* No row is a check error at any of these source lines. */
static int clean_lines(const cfx *f, const int *lines, size_t n)
{
    size_t i, k;

    for (i = 0; i < f->cdn; i++) {
        if (f->cds[i].severity != EK_SEV_ERROR) {
            continue;
        }
        for (k = 0; k < n; k++) {
            if (f->cds[i].line == lines[k]) {
                return 0;
            }
        }
    }
    return 1;
}

/* A std constructor anchors to its own type; with no user constructor
   of the name, an unannotated one self-anchors. */
static void std_only(void)
{
    cfx f;

    cfx_begin(&f,
              "x: Bool = True\n"
              "y: Bool = False\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f, "t = True\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

/* A user True and a std True are two constructors; each annotation
   selects its own. */
static void user_and_std(void)
{
    cfx f;

    cfx_begin(&f,
              "MyBool: type = True | False\n"
              "\n"
              "x: Bool = True\n"
              "y: MyBool = True\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

/* Two visible constructors and no anchor: undetermined, and the
   annotation resolves it rather than a default. */
static void unanchored(void)
{
    cfx f;
    static const int lines[] = {3, 4};

    cfx_begin(&f,
              "MyBool: type = True | False\n"
              "\n"
              "z := True\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-undetermined", 3));
    cfx_end(&f);

    cfx_begin(&f,
              "MyBool: type = True | False\n"
              "\n"
              "x: Bool = True\n"
              "y: MyBool = True\n"
              "z = True\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-undetermined", 5));
    EK_CHECK(clean_lines(&f, lines, 2));
    cfx_end(&f);
}

/* Two user constructors named True: the anchor selects, not source
   order. */
static void two_user_trues(void)
{
    cfx f;

    cfx_begin(&f,
              "Other: type = True | False\n"
              "MyBool: type = True | False\n"
              "x: MyBool = True\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

/* A user Ok and the std Ok; each annotation selects its own. */
static void two_oks(void)
{
    cfx f;

    cfx_begin(&f,
              "MyResult: type = Ok String | Retry Int\n"
              "\n"
              "a: MyResult = Ok \"once\"\n"
              "b: Result Int _ = Ok 3\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

/* The scrutinee type supplies a pattern's constructor set: a case
   over the user ADT needs its arms. */
static void user_scrutinee(void)
{
    cfx f;

    cfx_begin(&f,
              "MyBool: type = True | False\n"
              "\n"
              "b: MyBool = True\n"
              "f: (b: MyBool) -> String =\n"
              "    case b:\n"
              "        True -> \"yes\"\n"
              "        False -> \"no\"\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "MyBool: type = True | False\n"
              "f: (b: MyBool) -> Int =\n"
              "    case b:\n"
              "        True -> 1\n");
    EK_CHECK(cfx_code_at_line(&f, "E-exhaust", 3));
    cfx_end(&f);

    /* A same-named constructor of another type does not stand in for
       the scrutinee's own. */
    cfx_begin(&f,
              "Other: type = True | False\n"
              "MyBool: type = True | False\n"
              "f: (b: MyBool) -> String =\n"
              "    case b:\n"
              "        True -> \"yes\"\n"
              "        False -> \"no\"\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

/* A case over std Bool stays exhaustive while a user MyBool with the
   same constructor names is visible. */
static void std_scrutinee(void)
{
    cfx f;

    cfx_begin(&f,
              "MyBool: type = True | False\n"
              "\n"
              "f: (b: Bool) -> String =\n"
              "    case b:\n"
              "        True -> \"yes\"\n"
              "        False -> \"no\"\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

/* An anchored reference whose candidates do not include the expected
   type is a mismatch at the reference. */
static void anchor_mismatch(void)
{
    cfx f;

    cfx_begin(&f,
              "A: type = True | False\n"
              "B: type = Other\n"
              "x: B = True\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-nominal-mismatch", 3));
    cfx_end(&f);
}

/* The qualified spelling names the opened std type even when a user
   type of that spelling shadows the bare name. */
static void qualified_std_type(void)
{
    cfx f;

    cfx_begin(&f,
              "Bool: type = True | False\n"
              "y: std.Bool = True\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* The nominal std type rejects a literal of another type. */
    cfx_begin(&f,
              "Bool: type = True | False\n"
              "x: std.Bool = 3\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-nominal-mismatch", 2));
    cfx_end(&f);

    /* A name the placeholder does not declare keeps the bridge. */
    cfx_begin(&f, "x: std.Nope = 1\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

/* The payload of a constructor application is placed and conformed to
   the instantiated payload type, like a call argument. */
static void payload_conforms(void)
{
    cfx f;

    cfx_begin(&f,
              "T: type = Same Int\n"
              "g: (s: String) -> T = Same s\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-nominal-mismatch", 2));
    cfx_end(&f);

    cfx_begin(&f,
              "Wrap: type a = Wrap a\n"
              "g: (n: Int) -> Wrap String = Wrap n\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-nominal-mismatch", 2));
    cfx_end(&f);

    cfx_begin(&f,
              "T: type = Same Int\n"
              "Wrap: type a = Wrap a\n"
              "a: T = Same 1\n"
              "b: Wrap String = Wrap \"s\"\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

/* A qualified constructor application is typed by the constructor,
   not by the opaque member reading. */
static void qualified_application(void)
{
    static const char *const mpath[2] = {CONF_DIR, CTOR_DIR};
    cfx f;

    EK_CHECK(put_module("qliba.eko",
                        "ABool: type = True | False\n"));
    EK_CHECK(put_module("qwrap.eko",
                        "Wrap: type a = Wrap a\n"));

    /* A zero-payload constructor takes no argument; the empty call is
       the qualified spelling. */
    cfx_run(&f,
            "import qliba\n"
            "a: ABool = qliba.True()\n"
            "b: ABool = qliba.True\n",
            mpath, 2, "qapp1.eko");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK_U64(cfx_stage_errors(f.ds, f.dn), 0);
    cfx_end(&f);

    cfx_run(&f,
            "import qliba\n"
            "a: ABool = qliba.True(1)\n",
            mpath, 2, "qapp2.eko");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-call-arity", 2));
    cfx_end(&f);

    /* A payload constructor takes one argument, conformed at the
       payload type. */
    cfx_run(&f,
            "import geolib\n"
            "a: Shape = geolib.Circle(2.0)\n"
            "b: Shape = geolib.Circle(\"s\")\n",
            mpath, 2, "qapp3.eko");
    EK_CHECK_U64(cfx_stage_errors(f.ds, f.dn), 0);
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-nominal-mismatch", 3));
    cfx_end(&f);

    cfx_run(&f,
            "import geolib\n"
            "a: Shape = geolib.Circle(1.0, 2.0, 3.0)\n",
            mpath, 2, "qapp4.eko");
    EK_CHECK_U64(cfx_stage_errors(f.ds, f.dn), 0);
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-call-arity", 2));
    cfx_end(&f);

    /* A generic constructor takes its instantiation from the anchor;
       the qualified type spelling names the imported type. */
    cfx_run(&f,
            "import qwrap\n"
            "a: qwrap.Wrap String = qwrap.Wrap(\"s\")\n"
            "b: qwrap.Wrap String = qwrap.Wrap(3)\n",
            mpath, 2, "qapp5.eko");
    EK_CHECK_U64(cfx_stage_errors(f.ds, f.dn), 0);
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-nominal-mismatch", 3));
    cfx_end(&f);

    cfx_run(&f,
            "import qwrap\n"
            "v := qwrap.Wrap(3)\n",
            mpath, 2, "qapp6.eko");
    EK_CHECK_U64(cfx_stage_errors(f.ds, f.dn), 0);
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-undetermined", 2));
    cfx_end(&f);

    /* A std placeholder constructor keeps the bridge opacity. */
    cfx_run(&f, "a: Option Int = std.Some(3)\n", mpath, 2,
            "qapp7.eko");
    EK_CHECK_U64(cfx_stage_errors(f.ds, f.dn), 0);
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

/* A variant declared twice is rejected once by resolve, and the
   declaration still drains through check without a second defect. */
static void duplicate_variants(void)
{
    cfx f;

    cfx_begin(&f,
              "T: type = Dup Int | Dup String\n"
              "x: T = Dup 1\n"
              "f: (t: T) -> Int =\n"
              "    case t:\n"
              "        Dup n -> n + 1\n");
    EK_CHECK_U64(cfx_stage_errors(f.ds, f.dn), 1);
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

/* The message of the first check diagnostic at a line. */
static const char *diag_message(const cfx *f, int line)
{
    size_t i;

    for (i = 0; i < f->cdn; i++) {
        if (f->cds[i].line == line) {
            return f->cds[i].message;
        }
    }
    return NULL;
}

/* A std candidate in the owner list carries its module, so a user type
   of the same spelling stays distinguishable from it. */
static void undetermined_owner_text(void)
{
    cfx f;

    cfx_begin(&f,
              "Bool: type = True | False\n"
              "\n"
              "z := True\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(f.cdn == 1);
    EK_CHECK_STR(diag_message(&f, 3),
                 "two constructors named `True` are visible and no "
                 "expected type anchors the reference; write a `Bool or "
                 "std.Bool` annotation (Clause 7.4.6)");
    cfx_end(&f);
}

int main(void)
{
    std_only();
    user_and_std();
    unanchored();
    two_user_trues();
    two_oks();
    user_scrutinee();
    std_scrutinee();
    anchor_mismatch();
    qualified_std_type();
    payload_conforms();
    qualified_application();
    duplicate_variants();
    undetermined_owner_text();
    return ek_test_report("check/ctors");
}
