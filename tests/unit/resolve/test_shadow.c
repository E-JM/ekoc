#include "common.h"

#include <sys/stat.h>

/* A type binding spelled like a std open type shadows the implicit
   import's fallback layer, while constructors of the same spelling
   coexist: every visible candidate reaches the reference and check
   anchors it later. */

#define CONF_DIR "build/tmp-resolve/shadow"

static void put_fixtures(void)
{
    mkdir("build/tmp-resolve", 0755);
    mkdir(CONF_DIR, 0755);
    EK_CHECK(rfx_put(CONF_DIR, "res.eko",
        "Res: type = Ok Int | Err String\n"));
}

static void run_in(rfx *f, const char *src, const char *name)
{
    const char *path[1] = { CONF_DIR };

    rfx_run(f, src, path, 1, name);
}

/* A local constructor of a std constructor's name is an additional
   candidate; the first recorded symbol is the local one. */
static void local_ctors_coexist_with_std(void)
{
    rfx f;
    const ek_ref *r;

    rfx_begin(&f,
        "Opt: type = Some Int | None\n"
        "pick: (o: Opt) -> Int =\n"
        "    case o:\n"
        "        Some x -> x\n"
        "        None -> 0\n"
        "v: Opt = Some 1\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named(&f, 4, "Some");
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_CONSTRUCTOR);
    EK_CHECK(r->station == EK_ST_CONS);
    EK_CHECK_STR(r->symbol ? r->symbol->module : NULL, "test");
    EK_CHECK(r->symbol ? r->symbol->decl != NULL : 0);
    r = rfx_ref_named(&f, 6, "Some");
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_CONSTRUCTOR);
    EK_CHECK_STR(r->symbol ? r->symbol->module : NULL, "test");
    rfx_end(&f);

    rfx_begin(&f,
        "Res: type = Ok Int | Err String\n"
        "good: Res = Ok 1\n"
        "bad: Res = Err \"x\"\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named(&f, 2, "Ok");
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_CONSTRUCTOR);
    EK_CHECK_STR(r->symbol ? r->symbol->module : NULL, "test");
    r = rfx_ref_named(&f, 3, "Err");
    EK_CHECK(r != NULL);
    EK_CHECK_STR(r->symbol ? r->symbol->module : NULL, "test");
    rfx_end(&f);
}

/* A local type shadowing a std open type: the annotation binds the
   local declaration. */
static void local_type_shadows_std(void)
{
    rfx f;
    const ek_ref *r;

    rfx_begin(&f,
        "Clock: type = [ticks: Int]\n"
        "now: Clock = {ticks: 0}\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_at(&f, 2);
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_TYPE);
    EK_CHECK_STR(r->symbol ? r->symbol->module : NULL, "test");
    rfx_end(&f);

    /* Time is both a std type and a std constructor; one local
       declaration shadows both readings. */
    rfx_begin(&f,
        "Time: type = [ms: Int]\n"
        "t: Time = {ms: 1}\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_at(&f, 2);
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_TYPE);
    EK_CHECK_STR(r->symbol ? r->symbol->module : NULL, "test");
    rfx_end(&f);
}

/* Without a local constructor of the name, the std placeholder is the
   only candidate, so the bridge reading stays. */
static void std_candidate_without_a_local_one(void)
{
    rfx f;
    const ek_ref *r;

    rfx_begin(&f,
        "Opt: type = Some Int | None\n"
        "r: Result Int String = Ok 1\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named(&f, 2, "Ok");
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_EXTERNAL);
    EK_CHECK(r->station == EK_ST_EXTERNAL);
    EK_CHECK_STR(r->symbol ? r->symbol->module : NULL, "std");
    rfx_end(&f);
}

/* Two of the file's own constructors with one name coexist at
   resolve: the use site is anchored later, by the scrutinee type. */
static void duplicate_local_ctors_coexist(void)
{
    rfx f;
    const ek_ref *r;

    rfx_begin(&f,
        "A: type = Ok Int\n"
        "B: type = Ok String\n"
        "r: Int =\n"
        "    case 0:\n"
        "        Ok -> 1\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named(&f, 5, "Ok");
    EK_CHECK(r != NULL);
    EK_CHECK_U64(r->ncands, 3);
    EK_CHECK_STR(r->cands[0]->owner, "A");
    EK_CHECK_STR(r->cands[1]->owner, "B");
    EK_CHECK_STR(r->cands[2]->owner, "Result");
    rfx_end(&f);
}

/* An importing file sees a shadowed name through the exporter's
   surface, not the std layer. */
static void cross_module_shadow(void)
{
    rfx f;
    const ek_ref *r;

    run_in(&f,
        "import res\n"
        "\n"
        "r: Res Int String = Ok 1\n", "use.eko");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named(&f, 3, "Ok");
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_CONSTRUCTOR);
    EK_CHECK(r->station == EK_ST_CONS);
    EK_CHECK_STR(r->symbol ? r->symbol->module : NULL, "res");
    EK_CHECK(r->symbol ? r->symbol->decl != NULL : 0);
    rfx_end(&f);
}

int main(void)
{
    put_fixtures();
    local_ctors_coexist_with_std();
    local_type_shadows_std();
    std_candidate_without_a_local_one();
    duplicate_local_ctors_coexist();
    cross_module_shadow();
    return ek_test_report("resolve/shadow");
}
