#include "common.h"

/* String interpolation (Clause 5.9.3): slot runs parsed with the
   expression grammar, nesting, and escaped braces. */

/* Parses `m: String = <literal>` and returns the string node. */
static const ek_node *str_of(parse_fixture *f, const char *quoted)
{
    char src[256];

    snprintf(src, sizeof src, "m: String = %s\n", quoted);
    pfx_begin(f, src);
    return pfx_item(f, 0)->u.bind.value;
}

/* Literal segments are length-delimited bytes, not C strings. */
static inline int seg_is(const ek_str_seg *seg, const char *bytes, int len)
{
    return seg->text != NULL && seg->text_len == (size_t)len
        && memcmp(seg->text, bytes, (size_t)len) == 0;
}

static void plain_segments(void)
{
    parse_fixture f;
    const ek_node *s;

    s = str_of(&f, "\"\"");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(s->u.str.count, 0);
    pfx_end(&f);

    s = str_of(&f, "\"hi\"");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(s->u.str.count, 1);
    EK_CHECK_U64(s->u.str.segs[0].text != NULL, 1);
    EK_CHECK_U64(s->u.str.segs[0].text_len, 2);
    EK_CHECK_U64(s->u.str.segs[0].expr, NULL);
    pfx_end(&f);

    /* `\{` cooks to a literal brace, with no interpolation. */
    s = str_of(&f, "\"\\{x}\"");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(s->u.str.count, 1);
    EK_CHECK_U64(s->u.str.segs[0].text != NULL, 1);
    EK_CHECK(seg_is(&s->u.str.segs[0], "{x}", 3));
    pfx_end(&f);
}

static void slots(void)
{
    parse_fixture f;
    const ek_node *s;

    /* Literal, slot, literal (Clause 5.9.3). */
    s = str_of(&f, "\"a{x}b\"");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(s->u.str.count, 3);
    EK_CHECK(seg_is(&s->u.str.segs[0], "a", 1));
    EK_CHECK_U64(s->u.str.segs[1].text, NULL);
    EK_CHECK_U64(s->u.str.segs[1].expr->kind, EK_N_NAME);
    EK_CHECK_STR(s->u.str.segs[1].expr->u.path.data[0], "x");
    EK_CHECK(seg_is(&s->u.str.segs[2], "b", 1));
    pfx_end(&f);

    /* A slot holds a full expression. */
    s = str_of(&f, "\"n={n + 1}\"");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(s->u.str.segs[1].expr->kind, EK_N_BINARY);
    pfx_end(&f);

    /* Calls and fields inside a slot. */
    s = str_of(&f, "\"{p.name}: {xs.length()}\"");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(s->u.str.count, 3);
    EK_CHECK(seg_is(&s->u.str.segs[1], ": ", 2));
    EK_CHECK_U64(s->u.str.segs[0].expr->kind, EK_N_FIELD);
    EK_CHECK_U64(s->u.str.segs[2].expr->kind, EK_N_CALL);
    pfx_end(&f);

    /* Slots nest through string literals (Clause 5.9.3). */
    s = str_of(&f, "\"outer {\"inner {n}\"}\"");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(s->u.str.count, 2);
    EK_CHECK_U64(s->u.str.segs[1].expr->kind, EK_N_STRING_LIT);
    EK_CHECK_U64(s->u.str.segs[1].expr->u.str.count, 2);
    EK_CHECK_U64(s->u.str.segs[1].expr->u.str.segs[1].expr->kind, EK_N_NAME);
    pfx_end(&f);
}

static void slot_errors(void)
{
    parse_fixture f;

    /* A slot holds exactly one expression. */
    (void)str_of(&f, "\"a{x; y}b\"");
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_collect(&f);
    EK_CHECK(pfx_msg_has(&f, "5.9.3"));
    pfx_end(&f);
}

static void slot_positions(void)
{
    parse_fixture f;
    const ek_node *s;

    /* A slot node carries the position of its first token. */
    pfx_begin(&f, "m: String =\n    \"x={x}\"\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    s = pfx_item(&f, 0)->u.bind.value;
    EK_CHECK_U64(s->kind, EK_N_BLOCK); /* block RHS: unwrap the member */
    s = s->u.kids.data[0];
    EK_CHECK_U64(s->kind, EK_N_STRING_LIT);
    EK_CHECK_U64(s->u.str.segs[1].expr->line, 2);
    EK_CHECK(s->u.str.segs[1].expr->col > 4);
    pfx_end(&f);
}

static void slot_forms(void)
{
    parse_fixture f;
    const ek_node *s;

    /* A slot re-enters the whole expression grammar: lambda (A.3.4). */
    s = str_of(&f, "\"{(n): n + 1}\"");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(s->u.str.count, 1);
    EK_CHECK_U64(s->u.str.segs[0].expr->kind, EK_N_LAMBDA);
    pfx_end(&f);

    /* A slot holding an if (Clause 6.5.1: the whole slot is it). */
    s = str_of(&f, "\"{if c: 1 else: 2}\"");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(s->u.str.segs[0].expr->kind, EK_N_IF);
    pfx_end(&f);

    /* Braces balance inside a slot (Clause 5.9.3). */
    s = str_of(&f, "\"{ {1, 2} }\"");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(s->u.str.count, 1);
    EK_CHECK_U64(s->u.str.segs[0].expr->kind, EK_N_COMPOSITE);
    EK_CHECK_U64(s->u.str.segs[0].expr->u.kids.count, 2);
    pfx_end(&f);

    /* A slot holding a case with two inline arms (Clause 6.7.5). */
    s = str_of(&f, "\"{case x: 1 -> 2; _ -> 3}\"");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(s->u.str.segs[0].expr->kind, EK_N_CASE);
    EK_CHECK_U64(s->u.str.segs[0].expr->u.caseexpr.arms.count, 2);
    pfx_end(&f);
}

int main(void)
{
    plain_segments();
    slots();
    slot_errors();
    slot_positions();
    slot_forms();
    return ek_test_report("parse/interp");
}
