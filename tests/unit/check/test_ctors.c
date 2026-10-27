#include "common.h"

/* Anchored constructors: same-named constructors of different types
   coexist; an expected type anchors a value reference and the
   scrutinee type anchors a pattern. A reference with no anchor is
   legal only when exactly one constructor of its name is visible and
   that constructor is non-generic. */

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
    return ek_test_report("check/ctors");
}
