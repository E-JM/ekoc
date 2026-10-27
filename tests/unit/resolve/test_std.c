#include "common.h"
#include "refcheck.h"

/* The std surface: the compiled std module is parsed from lib/std.eko,
   so its declared types, their constructors, and its free names are
   ordinary declarations; the scalar, container, and capability type
   names are the placeholder rows beneath it. A name no layer declares
   is the unresolved-external bridge (resolve.h). */

/* Any bridged reference at a source line: the external station with no
   symbol behind it. */
static int bridge_at(const rfx *f, int line)
{
    size_t i;

    for (i = 0; i < f->res.ref_count; i++) {
        const ek_ref *r = &f->res.refs[i];

        if (r->node->line == line && r->kind == EK_SYM_EXTERNAL
            && r->station == EK_ST_EXTERNAL && r->symbol == NULL) {
            return 1;
        }
    }
    return 0;
}

static void free_names(void)
{
    rfx f;
    const ek_ref *r;

    rfx_begin(&f,
        "f: () -> _ =\n"
        "    assert_eq(1, 1)\n"
        "    _\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named(&f, 2, "assert_eq");
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_FUNC);
    EK_CHECK(r->station == EK_ST_HEAD_TOP);
    EK_CHECK_STR(r->symbol ? r->symbol->name : NULL, "assert_eq");
    EK_CHECK_STR(r->symbol ? r->symbol->module : NULL, "std");
    EK_CHECK(r->symbol != NULL && r->symbol->decl != NULL);
    rfx_end(&f);
}

/* The injected list opens assert, assert_eq, and assert_ne; a bare
   call to one resolves through the opened table. */
static void injected_opens(void)
{
    rfx f;
    const ek_ref *r;

    rfx_begin(&f,
        "f: () -> _ =\n"
        "    assert(True)\n"
        "    _\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named(&f, 2, "assert");
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_FUNC);
    EK_CHECK(r->station == EK_ST_HEAD_TOP);
    EK_CHECK_STR(r->symbol ? r->symbol->module : NULL, "std");
    EK_CHECK(r->symbol != NULL && r->symbol->decl != NULL);
    rfx_end(&f);
}

/* The std module joins the closure as a parsed module: its file is the
   release spelling, its AST is present, and its surface holds both the
   file's own declarations and the placeholder rows. The core types and
   their constructors are file rows. */
static void std_module_surface(void)
{
    rfx f;
    const ek_symbol *assert_sym;
    const ek_symbol *bool_sym;
    const ek_symbol *true_sym;

    rfx_begin(&f, "v: Int = 1\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    EK_CHECK_U64(f.res.module_count, 3); /* the root, std, and cap */
    if (f.res.module_count < 2) {
        rfx_end(&f);
        return;
    }
    EK_CHECK_STR(f.res.modules[1].module, "std");
    EK_CHECK_STR(f.res.modules[1].file, "std.eko");
    EK_CHECK(f.res.modules[1].ast != NULL);
    assert_sym = rfx_surface_sym(&f, "std", "assert");
    bool_sym = rfx_surface_sym(&f, "std", "Bool");
    true_sym = rfx_surface_sym(&f, "std", "True");
    EK_CHECK(assert_sym != NULL && assert_sym->decl != NULL);
    EK_CHECK(bool_sym != NULL && bool_sym->decl != NULL);
    EK_CHECK(true_sym != NULL && true_sym->decl != NULL);
    EK_CHECK(true_sym != NULL && true_sym->owner != NULL
             && strcmp(true_sym->owner, "Bool") == 0);
    rfx_end(&f);
}

static void opened_types(void)
{
    rfx f;
    const ek_ref *r;

    rfx_begin(&f,
        "f: String = \"x\"\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_at(&f, 1); /* the annotation resolves first */
    EK_CHECK(r != NULL);
    EK_CHECK_STR(r->symbol ? r->symbol->name : NULL, "String");
    EK_CHECK_STR(r->symbol ? r->symbol->module : NULL, "std");
    rfx_end(&f);
}

/* A core-type constructor is a file declaration, so the reference is a
   constructor reference, not the external bridge. */
static void std_constructors(void)
{
    rfx f;
    const ek_ref *r;

    rfx_begin(&f,
        "f: Result Int String = Ok 1\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named(&f, 1, "Ok");
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_CONSTRUCTOR);
    EK_CHECK(r->station == EK_ST_CONS);
    EK_CHECK_STR(r->symbol ? r->symbol->module : NULL, "std");
    EK_CHECK(r->symbol != NULL && r->symbol->decl != NULL);
    EK_CHECK(r->symbol != NULL && r->symbol->owner != NULL
             && strcmp(r->symbol->owner, "Result") == 0);
    rfx_end(&f);
}

/* A name the placeholder does not know: an unknown bare name is
   unbound; an unknown member of a std type is the bridge
   (resolve.h). */
static void unknown_names(void)
{
    rfx f;

    rfx_begin(&f,
        "f: Int = nothere\n");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-unbound-name", 1));
    rfx_end(&f);

    rfx_begin(&f,
        "f: () -> Int = String.unknown_thing()\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    /* A qualified Pascal head is the type-namespace reading; the
       placeholder knows the type, check owns its members. */
    {
        const ek_ref *r = rfx_ref_named(&f, 1, "String");

        EK_CHECK(r != NULL);
        EK_CHECK(r->kind == EK_SYM_TYPE);
    }
    rfx_end(&f);
}

static void qualified_std(void)
{
    rfx f;

    /* A qualified spelling of a name the import bridge leaves unknown
       keeps the placeholder reading. */
    rfx_begin(&f,
        "f: () -> _ =\n"
        "    std.show(1)\n"
        "    _\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    rfx_end(&f);
}

/* The qualified spelling denotes the file's declaration, so the call
   carries the real arrow. */
static void qualified_real_function(void)
{
    rfx f;
    const ek_ref *r;

    rfx_begin(&f,
        "f: () -> _ =\n"
        "    std.assert(True)\n"
        "    std.assert_eq(1, 2)\n"
        "    _\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named(&f, 2, "assert");
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_FUNC);
    EK_CHECK(r->station == EK_ST_QUALIFIED);
    EK_CHECK(r->symbol != NULL && r->symbol->decl != NULL);
    r = rfx_ref_named(&f, 3, "assert_eq");
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_FUNC);
    EK_CHECK(r->station == EK_ST_QUALIFIED);
    EK_CHECK(r->symbol != NULL && r->symbol->decl != NULL);
    rfx_end(&f);
}

/* A std member the file does not declare keeps the external bridge, not
   a new diagnostic. */
static void std_unknown_member(void)
{
    rfx f;

    rfx_begin(&f,
        "f: () -> _ =\n"
        "    std.nope(1)\n"
        "    _\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    EK_CHECK(bridge_at(&f, 2));
    rfx_end(&f);
}

/* The std module's own body resolves through the ordinary layers: its
   reference sites are covered by the refs table, so the module is a
   parsed module and not only a surface row. */
static void std_body_resolved(void)
{
    rfx f;
    const ek_node *std_ast;
    size_t i, own = 0;

    rfx_begin(&f, "v: Int = 1\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    EK_CHECK_U64(f.res.module_count, 3);
    if (f.res.module_count < 2) {
        rfx_end(&f);
        return;
    }
    std_ast = f.res.modules[1].ast;
    for (i = 0; i < f.res.ref_count; i++) {
        if ((size_t)f.res.refs[i].node > (size_t)std_ast) {
            own++;
        }
    }
    EK_CHECK(own > 0);
    EK_CHECK_U64(rfx_check_refs(&f), 0);
    rfx_end(&f);
}

/* The boot is deterministic: two resolutions of one source yield the
   same closure and the same std surface, in the same order. */
static void boot_deterministic(void)
{
    rfx a, b;
    size_t i;

    rfx_begin(&a, "v: Int = 1\n");
    rfx_begin(&b, "v: Int = 1\n");
    EK_CHECK_U64(a.res.module_count, b.res.module_count);
    if (a.res.module_count == b.res.module_count
        && a.res.module_count >= 2) {
        const ek_surface *x = &a.res.modules[1];
        const ek_surface *y = &b.res.modules[1];

        EK_CHECK_U64(x->count, y->count);
        if (x->count == y->count) {
            for (i = 0; i < x->count; i++) {
                EK_CHECK(x->syms[i].kind == y->syms[i].kind);
                EK_CHECK_STR(x->syms[i].name, y->syms[i].name);
            }
        }
    }
    rfx_end(&a);
    rfx_end(&b);
}

/* A service free name that no layer binds is the receiver form of the
   compiler service; it is not a std surface row. show, inspect, and
   panic all take that reading. */
static void service_identity(void)
{
    rfx f;
    const ek_ref *r;

    rfx_begin(&f,
        "f: () -> _ =\n"
        "    show(1)\n"
        "    inspect(1)\n"
        "    panic(\"x\")\n"
        "    _\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_at(&f, 2);
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_MEMBER);
    EK_CHECK(r->station == EK_ST_RECEIVER);
    EK_CHECK(r->symbol == NULL);
    r = rfx_ref_at(&f, 3);
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_MEMBER);
    EK_CHECK(r->station == EK_ST_RECEIVER);
    r = rfx_ref_at(&f, 4);
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_MEMBER);
    EK_CHECK(r->station == EK_ST_RECEIVER);
    rfx_end(&f);
}

/* A service's spelling is not an opened name, so a top-level binding
   of it is legal and owns the name. */
static void top_level_service_name(void)
{
    rfx f;
    const ek_ref *r;

    rfx_begin(&f,
        "show: Int = 1\n"
        "v: Int = show\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named_kind(&f, 2, "show", EK_SYM_FUNC);
    EK_CHECK(r != NULL);
    EK_CHECK(r->station == EK_ST_HEAD_TOP);
    rfx_end(&f);
}

/* The qualified spelling is legal at every name position, including a
   pattern constructor of a core type. */
static void qualified_std_constructors(void)
{
    rfx f;
    const ek_ref *r;

    rfx_begin(&f,
        "f: (b: Bool) -> Int =\n"
        "    case b:\n"
        "        std.True -> 1\n"
        "        std.False -> 0\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named(&f, 3, "True");
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_CONSTRUCTOR);
    EK_CHECK(r->station == EK_ST_QUALIFIED);
    EK_CHECK_STR(r->symbol ? r->symbol->module : NULL, "std");
    EK_CHECK(r->symbol != NULL && r->symbol->decl != NULL);
    rfx_end(&f);
}

/* The qualified spelling of an opened std type names that type's file
   row even when a user type shadows the bare name; a name the
   placeholder does not declare stays the bridge. */
static void qualified_std_types(void)
{
    rfx f;
    const ek_ref *r;

    rfx_begin(&f,
        "Bool: type = True | False\n"
        "x: std.Bool = True\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named_kind(&f, 2, "Bool", EK_SYM_TYPE);
    EK_CHECK(r != NULL);
    EK_CHECK(r->station == EK_ST_QUALIFIED);
    EK_CHECK_STR(r->symbol ? r->symbol->module : NULL, "std");
    EK_CHECK(r->symbol != NULL && r->symbol->decl != NULL);
    rfx_end(&f);

    rfx_begin(&f, "z: std.Nope = 1\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_at(&f, 1);
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_EXTERNAL);
    EK_CHECK(r->station == EK_ST_TYPE);
    rfx_end(&f);
}

/* Local shadowing of an opened free name is legal; only a top-level
   binding collides with the layer it occupies. */
static void local_shadow_free_name(void)
{
    rfx f;
    const ek_ref *r;

    rfx_begin(&f,
        "f: () -> _ =\n"
        "    show: Int = 1\n"
        "    show\n"
        "    _\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named(&f, 3, "show");
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_LOCAL);
    EK_CHECK(r->station == EK_ST_HEAD_LOCAL);
    rfx_end(&f);
}

/* The fixed forms of the standard library are file declarations: the
   constructors of the time and failure types are rows of the declaring
   file, not placeholder rows, and each type has exactly one row. */
static void declared_std_types(void)
{
    rfx f;
    const ek_ref *r;
    size_t i, k, ctors = 0;
    int std_seen = 0;

    rfx_begin(&f,
        "p: ParseError = Invalid \"x\"\n"
        "j: Json = Obj {\"a\": Null}\n"
        "t: Time = Time 5\n"
        "m: Mono = Mono 0\n"
        "d: Duration = Duration 3\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named(&f, 1, "Invalid");
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_CONSTRUCTOR);
    EK_CHECK(r->symbol != NULL && r->symbol->decl != NULL);
    EK_CHECK(r->symbol != NULL && r->symbol->owner != NULL
             && strcmp(r->symbol->owner, "ParseError") == 0);
    r = rfx_ref_named(&f, 2, "Obj");
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_CONSTRUCTOR);
    EK_CHECK(r->symbol != NULL && r->symbol->owner != NULL
             && strcmp(r->symbol->owner, "Json") == 0);
    r = rfx_ref_named_kind(&f, 3, "Time", EK_SYM_CONSTRUCTOR);
    EK_CHECK(r != NULL);
    EK_CHECK(r->symbol != NULL && r->symbol->decl != NULL);
    for (k = 0; k < f.res.module_count; k++) {
        const ek_surface *s = &f.res.modules[k];

        if (strcmp(s->module, "std") != 0) {
            continue;
        }
        std_seen = 1;
        for (i = 0; i < s->count; i++) {
            if (s->syms[i].kind == EK_SYM_CONSTRUCTOR
                && strcmp(s->syms[i].name, "Time") == 0) {
                ctors++;
            }
        }
    }
    EK_CHECK(std_seen);
    EK_CHECK_U64(ctors, 1); /* the file's row, no placeholder beside it */
    rfx_end(&f);
}

int main(void)
{
    free_names();
    injected_opens();
    std_module_surface();
    declared_std_types();
    std_body_resolved();
    boot_deterministic();
    qualified_real_function();
    std_unknown_member();
    opened_types();
    std_constructors();
    unknown_names();
    qualified_std();
    service_identity();
    top_level_service_name();
    qualified_std_constructors();
    qualified_std_types();
    local_shadow_free_name();
    return ek_test_report("resolve/std");
}
