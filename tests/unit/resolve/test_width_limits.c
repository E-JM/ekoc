#include "common.h"
#include "refcheck.h"

/* Binding patterns and receiver chains are not bounded by a fixed
   width: every name of a pattern takes part in the duplicate rule,
   and every link of a chain resolves with its chain classification. */

#define WIDE_NAMES 70
#define WIDE_LINKS 40

/* Builds a case-arm pattern binding WIDE_NAMES names. The name at
   position repeat_at repeats the name at position first_at; when the
   positions differ, *repeat_col receives the repeated name's column. */
static void wide_pattern_src(char *buf, size_t cap, size_t first_at,
                             size_t repeat_at, size_t *repeat_col)
{
    size_t at, names_at, i;

    at = (size_t)snprintf(buf, cap,
                          "f: (p: [Int, Int]) -> Int =\n"
                          "    case p:\n"
                          "        {");
    names_at = at;
    for (i = 0; i < WIDE_NAMES; i++) {
        const char *sep = i ? ", " : "";
        char name[16];

        if (i + 1 == repeat_at) {
            snprintf(name, sizeof name, "%sn%zu", sep, first_at - 1);
            *repeat_col = 10 + (at - names_at) + (i ? 2 : 0);
        } else {
            snprintf(name, sizeof name, "%sn%zu", sep, i);
        }
        at += (size_t)snprintf(buf + at, cap - at, "%s", name);
    }
    snprintf(buf + at, cap - at, "} -> 1\n");
}

static const ek_diagnostic *first_error(const rfx *f)
{
    size_t i;

    for (i = 0; i < f->dn; i++) {
        if (f->ds[i].severity == EK_SEV_ERROR) {
            return &f->ds[i];
        }
    }
    return NULL;
}

static const ek_ref *ref_at_node(const rfx *f, const ek_node *node)
{
    size_t i;

    for (i = 0; i < f->res.ref_count; i++) {
        if (f->res.refs[i].node == node) {
            return &f->res.refs[i];
        }
    }
    return NULL;
}

/* Seventy distinct names bind without a diagnostic. */
static void wide_pattern_distinct(void)
{
    char src[4096];
    size_t col = 0;
    rfx f;

    wide_pattern_src(src, sizeof src, 0, 0, &col);
    rfx_begin(&f, src);
    EK_CHECK_U64(rfx_errors(&f), 0);
    rfx_end(&f);
}

/* The name at position one repeats at position seventy: the conflict
   is diagnosed at the repeat. */
static void wide_pattern_repeat(void)
{
    char src[4096];
    size_t col = 0;
    const ek_diagnostic *d;
    rfx f;

    wide_pattern_src(src, sizeof src, 1, WIDE_NAMES, &col);
    rfx_begin(&f, src);
    EK_CHECK_U64(rfx_errors(&f), 1);
    d = first_error(&f);
    EK_CHECK(d != NULL);
    if (d) {
        EK_CHECK_STR(d->code, "E-name-conflict");
        EK_CHECK_U64(d->line, 3);
        EK_CHECK_U64(d->col, col);
    }
    rfx_end(&f);
}

/* A name first seen late in a long pattern still conflicts when it
   repeats. */
static void wide_pattern_repeat_late(void)
{
    char src[4096];
    size_t col = 0;
    const ek_diagnostic *d;
    rfx f;

    wide_pattern_src(src, sizeof src, 66, WIDE_NAMES, &col);
    rfx_begin(&f, src);
    EK_CHECK_U64(rfx_errors(&f), 1);
    d = first_error(&f);
    EK_CHECK(d != NULL);
    if (d) {
        EK_CHECK_STR(d->code, "E-name-conflict");
        EK_CHECK_U64(d->line, 3);
        EK_CHECK_U64(d->col, col);
    }
    rfx_end(&f);
}

/* A forty-link member chain resolves in one walk: the outermost link
   is the field reading and every inner link is a receiver reading. */
static void wide_chain(void)
{
    char src[1024];
    const ek_node *value, *p;
    size_t at, i, links = 0, receivers = 0;
    rfx f;

    at = (size_t)snprintf(src, sizeof src, "r: Int = 1\nx: Int = r.f1");
    for (i = 2; i <= WIDE_LINKS; i++) {
        at += (size_t)snprintf(src + at, sizeof src - at, ".f%zu", i);
    }
    snprintf(src + at, sizeof src - at, "\n");
    rfx_begin(&f, src);
    EK_CHECK_U64(rfx_errors(&f), 0);
    value = f.ast.module && f.ast.module->u.kids.count == 2
                ? f.ast.module->u.kids.data[1]->u.bind.value : NULL;
    EK_CHECK(value != NULL && value->kind == EK_N_FIELD);
    for (p = value; p && p->kind == EK_N_FIELD; p = p->u.field.recv) {
        const ek_ref *ref = ref_at_node(&f, p);

        links++;
        EK_CHECK(ref != NULL);
        if (!ref) {
            continue;
        }
        EK_CHECK(ref->kind == EK_SYM_MEMBER);
        EK_CHECK(ref->station == (links == 1 ? EK_ST_FIELD
                                             : EK_ST_RECEIVER));
        if (ref->station == EK_ST_RECEIVER) {
            receivers++;
        }
    }
    EK_CHECK_U64(links, WIDE_LINKS);
    EK_CHECK_U64(receivers, WIDE_LINKS - 1);
    EK_CHECK_U64(rfx_check_refs(&f), 0);
    rfx_end(&f);
}

int main(void)
{
    wide_pattern_distinct();
    wide_pattern_repeat();
    wide_pattern_repeat_late();
    wide_chain();
    return ek_test_report("resolve/width_limits");
}
