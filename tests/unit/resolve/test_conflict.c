#include "common.h"

#include <sys/stat.h>

/* Name conflicts. File-backed cases use one fixture directory under
   build/ created at test runtime. */

#define CONF_DIR "build/tmp-resolve/conf"

static void put_fixtures(void)
{
    mkdir("build/tmp-resolve", 0755);
    mkdir(CONF_DIR, 0755);
    EK_CHECK(rfx_put(CONF_DIR, "geolib.eko",
        "Shape: type = Circle Float\n"
        "pi: Float = 3.0\n"));
    EK_CHECK(rfx_put(CONF_DIR, "box.eko",
        "Box: type = [v: Int]\n"));
    EK_CHECK(rfx_put(CONF_DIR, "t1.eko",
        "P: type = [x: Int]\n"
        "mk: () -> P = {x: 1}\n"));
    EK_CHECK(rfx_put(CONF_DIR, "t2.eko",
        "P: type = [y: Int]\n"));
}

static void run_in(rfx *f, const char *src, const char *name)
{
    const char *path[1] = { CONF_DIR };

    rfx_run(f, src, path, 1, name);
}

/* N12: an import and a top-level binding share a name. */
static void n12(void)
{
    rfx f;

    run_in(&f,
        "import geolib\n"
        "\n"
        "geolib: Int = 1\n", "n12.eko");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-name-conflict", 3));
    rfx_end(&f);
}

static void duplicates(void)
{
    rfx f;

    rfx_begin(&f,
        "f: Int = 1\n"
        "f: String = \"x\"\n");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-name-conflict", 2));
    rfx_end(&f);

    /* The std free names share the top-level layer. */
    rfx_begin(&f,
        "assert: Int = 1\n");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-name-conflict", 1));
    rfx_end(&f);

    /* The implicit import is an import: a top-level `std` collides
       with it. */
    rfx_begin(&f,
        "std: Int = 1\n");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-name-conflict", 1));
    rfx_end(&f);
}

/* A qualified form carries one module segment; a longer dotted name
   denotes nothing. */
static void over_qualified_names(void)
{
    rfx f;

    rfx_begin(&f,
        "f: (x: a.b.C) -> Int = 0\n");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-unbound-name", 1));
    rfx_end(&f);

    rfx_begin(&f,
        "f: (n: Int) -> Int =\n"
        "    case n:\n"
        "        x.y.Z -> 1\n"
        "        _ -> 0\n");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-unbound-name", 3));
    rfx_end(&f);
}

static void namespace_heads(void)
{
    rfx f;

    /* A namespace binding is keyed by its full head, so Int.add and
       Float.add coexist. */
    rfx_begin(&f,
        "A: type = [x: Int]\n"
        "B: type = [y: Int]\n"
        "A.get: (a: A) -> Int = a.x\n"
        "B.get: (b: B) -> Int = b.y\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    rfx_end(&f);

    rfx_begin(&f,
        "A: type = [x: Int]\n"
        "A.get: (a: A) -> Int = a.x\n"
        "A.get: (a: A) -> Int = a.x\n");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-name-conflict", 3));
    rfx_end(&f);
}

static void pattern_repeats(void)
{
    rfx f;

    /* A binding name occurs at most once in one pattern. */
    rfx_begin(&f,
        "f: (p: [Int, Int]) -> Int =\n"
        "    case p:\n"
        "        {x, x} -> x\n");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-name-conflict", 3));
    rfx_end(&f);

    /* The rule applies separately to each or-alternative. */
    rfx_begin(&f,
        "f: (xs: []Int) -> Int =\n"
        "    case xs:\n"
        "        {a} | {.., a} -> a\n"
        "        _ -> 0\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    rfx_end(&f);

    /* A rest name is a binding name like any other. */
    rfx_begin(&f,
        "f: (xs: []Int) -> Int =\n"
        "    case xs:\n"
        "        {x, ..x} -> x\n"
        "        _ -> 0\n");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-name-conflict", 3));
    rfx_end(&f);
}

static void tvar_vs_module(void)
{
    rfx f;

    /* A type variable with the name of an imported module. */
    run_in(&f,
        "import box\n"
        "\n"
        "f: (v: box) -> Int = 0\n", "tvar.eko");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-name-conflict", 3));
    rfx_end(&f);
}

static void constructor_candidates(void)
{
    rfx f;
    const ek_ref *r;

    /* Same-named constructors of different types coexist; resolve
       hands both to check as the candidate set. */
    rfx_begin(&f,
        "A: type = Same Int\n"
        "B: type = Same Int\n"
        "f: Int =\n"
        "    case 0:\n"
        "        Same -> 1\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named(&f, 5, "Same");
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_CONSTRUCTOR);
    EK_CHECK(r->station == EK_ST_CONS);
    EK_CHECK_U64(r->ncands, 2);
    EK_CHECK_STR(r->cands[0]->owner, "A");
    EK_CHECK_STR(r->cands[1]->owner, "B");
    rfx_end(&f);
}

/* A variant name declared twice in one type is a declaration
   duplicate: one diagnostic at the second variant, and the surface
   carries the constructor once. */
static void duplicate_variants(void)
{
    rfx f;
    const ek_ref *r;

    rfx_begin(&f,
        "T: type = Dup Int | Dup String\n"
        "x: T = Dup 1\n");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-name-conflict", 1));
    EK_CHECK(f.dn == 1);
    EK_CHECK(f.dn == 1 && f.ds[0].code != NULL
             && strcmp(f.ds[0].code, "E-name-conflict") == 0);
    EK_CHECK(f.dn == 1 && f.ds[0].message != NULL
             && strcmp(f.ds[0].message,
                       "the constructor `Dup` is declared twice in the "
                       "type `T` (Clause 7.4.6)") == 0);
    r = rfx_ref_named(&f, 2, "Dup");
    EK_CHECK(r != NULL);
    EK_CHECK_U64(r->ncands, 1);
    rfx_end(&f);
}

static void same_named_types(void)
{
    rfx f;

    /* Same-named types coexist; the unqualified use is the point of
       rejection. */
    run_in(&f,
        "import t1\n"
        "import t2\n"
        "\n"
        "f: P = t1.mk()\n", "types.eko");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-name-conflict", 4));
    rfx_end(&f);
}

static void namespace_owner(void)
{
    rfx f;

    /* A namespace member is legal only in the declaring module. */
    rfx_begin(&f,
        "f: Int = 0\n"
        "String.foo: (s: String) -> Int = 0\n");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-namespace-owner", 2));
    rfx_end(&f);
}

static void name_case(void)
{
    rfx f;

    /* A value binding carries the snake_case case class. */
    rfx_begin(&f,
        "Bad: Int = 1\n");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-name-case", 1));
    /* The rejected spelling still records its constructor reading at
       the pattern position. */
    {
        size_t i;
        int seen = 0;

        for (i = 0; i < f.res.ref_count; i++) {
            if (f.res.refs[i].node->line == 1
                && f.res.refs[i].kind == EK_SYM_EXTERNAL) {
                seen = 1;
            }
        }
        EK_CHECK(seen);
    }
    rfx_end(&f);
}

int main(void)
{
    put_fixtures();
    n12();
    duplicates();
    over_qualified_names();
    namespace_heads();
    pattern_repeats();
    tvar_vs_module();
    constructor_candidates();
    duplicate_variants();
    same_named_types();
    namespace_owner();
    name_case();
    return ek_test_report("resolve/conflict");
}
