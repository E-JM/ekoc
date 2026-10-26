#include "common.h"

/* Patterns of Clause 6.8: variables, wildcard, literals, constructors,
   the shared brace forms, rests, and or-patterns. */

/* Parses `f: (v: T) -> Int = case v:` + one arm line. */
static const ek_node *arm_pattern(parse_fixture *f, const char *arm_line)
{
    char src[256];

    snprintf(src, sizeof src,
             "f: (v: Int) -> Int =\n    case v:\n        %s\n", arm_line);
    pfx_begin(f, src);
    return pfx_find(f->ast.module, EK_N_CASE);
}

static void simple_patterns(void)
{
    parse_fixture f;
    const ek_node *ce;
    const ek_node *p;

    ce = arm_pattern(&f, "x -> 1");
    EK_CHECK_U64(pfx_errors(&f), 0);
    p = ce->u.caseexpr.arms.data[0]->u.arm.patterns.data[0];
    EK_CHECK_U64(p->kind, EK_N_P_VAR);
    EK_CHECK_STR(p->u.path.data[0], "x");
    pfx_end(&f);

    ce = arm_pattern(&f, "_ -> 1");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(ce->u.caseexpr.arms.data[0]->u.arm.patterns.data[0]->kind,
                 EK_N_P_WILDCARD);
    pfx_end(&f);

    ce = arm_pattern(&f, "1.5 -> 1");
    EK_CHECK_U64(pfx_errors(&f), 0);
    p = ce->u.caseexpr.arms.data[0]->u.arm.patterns.data[0];
    EK_CHECK_U64(p->kind, EK_N_P_LIT);
    EK_CHECK_U64(p->u.wrap.inner->kind, EK_N_FLOAT_LIT);
    pfx_end(&f);

    ce = arm_pattern(&f, "'a' -> 1");
    EK_CHECK_U64(pfx_errors(&f), 0);
    p = ce->u.caseexpr.arms.data[0]->u.arm.patterns.data[0];
    EK_CHECK_U64(p->u.wrap.inner->kind, EK_N_CHAR_LIT);
    pfx_end(&f);

    ce = arm_pattern(&f, "\"man\" -> 1");
    EK_CHECK_U64(pfx_errors(&f), 0);
    p = ce->u.caseexpr.arms.data[0]->u.arm.patterns.data[0];
    EK_CHECK_U64(p->u.wrap.inner->kind, EK_N_STRING_LIT);
    EK_CHECK_U64(p->u.wrap.inner->u.str.count, 1);
    EK_CHECK(p->u.wrap.inner->u.str.segs[0].text != NULL);
    pfx_end(&f);

    /* Negative numeric (Clause 6.8.5). */
    ce = arm_pattern(&f, "-1 -> 1");
    EK_CHECK_U64(pfx_errors(&f), 0);
    p = ce->u.caseexpr.arms.data[0]->u.arm.patterns.data[0];
    EK_CHECK_U64(p->kind, EK_N_P_LIT);
    EK_CHECK_U64(p->u.wrap.inner->kind, EK_N_UNARY);
    pfx_end(&f);
}

static void constructor_patterns(void)
{
    parse_fixture f;
    const ek_node *ce;
    const ek_node *p;

    ce = arm_pattern(&f, "Man -> 1");
    EK_CHECK_U64(pfx_errors(&f), 0);
    p = ce->u.caseexpr.arms.data[0]->u.arm.patterns.data[0];
    EK_CHECK_U64(p->kind, EK_N_P_CONS);
    EK_CHECK_STR(p->u.cons.name.data[0], "Man");
    EK_CHECK_U64(p->u.cons.payload, NULL);
    pfx_end(&f);

    /* Literal payloads nest (Clause 6.8.5). */
    ce = arm_pattern(&f, "Err \"timeout\" -> 1");
    EK_CHECK_U64(pfx_errors(&f), 0);
    p = ce->u.caseexpr.arms.data[0]->u.arm.patterns.data[0];
    EK_CHECK_U64(p->kind, EK_N_P_CONS);
    EK_CHECK_U64(p->u.cons.payload->kind, EK_N_P_LIT);
    pfx_end(&f);

    /* Variable payloads. */
    ce = arm_pattern(&f, "Ok n -> n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    p = ce->u.caseexpr.arms.data[0]->u.arm.patterns.data[0];
    EK_CHECK_U64(p->u.cons.payload->kind, EK_N_P_VAR);
    pfx_end(&f);

    /* A qualified constructor Mod.Name (Clause 7.4.6). */
    ce = arm_pattern(&f, "geo.Circle r -> 1");
    EK_CHECK_U64(pfx_errors(&f), 0);
    p = ce->u.caseexpr.arms.data[0]->u.arm.patterns.data[0];
    EK_CHECK_U64(p->kind, EK_N_P_CONS);
    EK_CHECK_U64(p->u.cons.name.count, 2);
    pfx_end(&f);
}

static void brace_patterns(void)
{
    parse_fixture f;
    const ek_node *ce;
    const ek_node *p;

    /* Empty braces are the empty-list reading (Clause 6.8.4). */
    ce = arm_pattern(&f, "{} -> 1");
    EK_CHECK_U64(pfx_errors(&f), 0);
    p = ce->u.caseexpr.arms.data[0]->u.arm.patterns.data[0];
    EK_CHECK_U64(p->kind, EK_N_P_BRACE);
    EK_CHECK_U64(p->u.p_brace.items.count, 0);
    EK_CHECK_U64(p->u.p_brace.rest, EK_REST_NONE);
    pfx_end(&f);

    /* Fixed length. */
    ce = arm_pattern(&f, "{x, y} -> x");
    EK_CHECK_U64(pfx_errors(&f), 0);
    p = ce->u.caseexpr.arms.data[0]->u.arm.patterns.data[0];
    EK_CHECK_U64(p->u.p_brace.items.count, 2);
    EK_CHECK_U64(p->u.p_brace.items.data[0]->kind, EK_N_P_VAR);
    pfx_end(&f);

    /* Suffix rest with a name. */
    ce = arm_pattern(&f, "{h, ..t} -> h");
    EK_CHECK_U64(pfx_errors(&f), 0);
    p = ce->u.caseexpr.arms.data[0]->u.arm.patterns.data[0];
    EK_CHECK_U64(p->u.p_brace.rest, EK_REST_SUFFIX);
    EK_CHECK_STR(p->u.p_brace.rest_name, "t");
    pfx_end(&f);

    /* Anonymous rest (Clause 6.8.4). */
    ce = arm_pattern(&f, "{_, ..} -> 1");
    EK_CHECK_U64(pfx_errors(&f), 0);
    p = ce->u.caseexpr.arms.data[0]->u.arm.patterns.data[0];
    EK_CHECK_U64(p->u.p_brace.rest, EK_REST_SUFFIX);
    EK_CHECK_U64(p->u.p_brace.rest_name, NULL);
    pfx_end(&f);

    /* Prefix rest: the last element is the pattern (Clause 6.8.4). */
    ce = arm_pattern(&f, "{.., 42} -> 1");
    EK_CHECK_U64(pfx_errors(&f), 0);
    p = ce->u.caseexpr.arms.data[0]->u.arm.patterns.data[0];
    EK_CHECK_U64(p->u.p_brace.rest, EK_REST_PREFIX);
    EK_CHECK_U64(p->u.p_brace.items.count, 1);
    EK_CHECK_U64(p->u.p_brace.items.data[0]->kind, EK_N_P_LIT);
    pfx_end(&f);

    /* Record field forms of Clause 6.8.2, mixed. */
    ce = arm_pattern(&f, "{name, age: a} -> a");
    EK_CHECK_U64(pfx_errors(&f), 0);
    p = ce->u.caseexpr.arms.data[0]->u.arm.patterns.data[0];
    EK_CHECK_U64(p->u.p_brace.items.data[0]->kind, EK_N_P_VAR);
    EK_CHECK_U64(p->u.p_brace.items.data[1]->kind, EK_N_P_FIELD);
    EK_CHECK_STR(p->u.p_brace.items.data[1]->u.p_field.name, "age");
    EK_CHECK_STR(p->u.p_brace.items.data[1]->u.p_field.pattern->u.path.data[0],
                 "a");
    pfx_end(&f);

    /* A constructor payload may be a record pattern (Clause 6.8.2). */
    ce = arm_pattern(&f, "Holder {name} -> name");
    EK_CHECK_U64(pfx_errors(&f), 0);
    p = ce->u.caseexpr.arms.data[0]->u.arm.patterns.data[0];
    EK_CHECK_U64(p->kind, EK_N_P_CONS);
    EK_CHECK_U64(p->u.cons.payload->kind, EK_N_P_BRACE);
    pfx_end(&f);
}

static void or_patterns(void)
{
    parse_fixture f;
    const ek_node *ce;
    const ek_node *p;

    ce = arm_pattern(&f, "Key Up | Key Down -> 1");
    EK_CHECK_U64(pfx_errors(&f), 0);
    p = ce->u.caseexpr.arms.data[0]->u.arm.patterns.data[0];
    EK_CHECK_U64(p->kind, EK_N_P_OR);
    EK_CHECK_U64(p->u.kids.count, 2);
    EK_CHECK_U64(p->u.kids.data[0]->kind, EK_N_P_CONS);
    EK_CHECK_U64(p->u.kids.data[0]->u.cons.payload->kind, EK_N_P_CONS);
    pfx_end(&f);
}

static void paren_patterns(void)
{
    parse_fixture f;
    const ek_node *let;

    /* The tuple split of Clause 6.3.2. */
    pfx_begin(&f, "f: Int =\n    (a, b) = row\n    a\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    let = pfx_item(&f, 0)->u.bind.value->u.kids.data[0];
    EK_CHECK_U64(let->kind, EK_N_LET);
    EK_CHECK_U64(let->u.bind.pattern->kind, EK_N_P_PAREN);
    EK_CHECK_U64(let->u.bind.pattern->u.kids.count, 2);
    pfx_end(&f);
}

static void excluded_patterns(void)
{
    parse_fixture f;

    /* An interpolated string is not a literal pattern (6.8.5). */
    (void)arm_pattern(&f, "\"x{n}\" -> 1");
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_collect(&f);
    EK_CHECK(pfx_has_code(&f, "E-interp-pattern"));
    pfx_end(&f);

    /* A negative pattern takes only a numeric literal. */
    (void)arm_pattern(&f, "-x -> 1");
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_end(&f);
}

/* Rest forms that A.3.6 does not admit must be diagnosed, not
   silently reshaped: one rest per brace pattern, and a suffix rest
   closes the brace. */
static void rest_edges(void)
{
    parse_fixture f;

    (void)arm_pattern(&f, "{.., 1, ..} -> 1");
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_collect(&f);
    EK_CHECK(pfx_msg_has(&f, "one rest"));
    pfx_end(&f);

    (void)arm_pattern(&f, "{1, ..t, 2} -> 1");
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_collect(&f);
    EK_CHECK(pfx_msg_has(&f, "expected `}` after the rest"));
    pfx_end(&f);

    /* The legal placements of Clause 6.8.4 stay clean. */
    (void)arm_pattern(&f, "{h, ..t} -> h");
    EK_CHECK_U64(pfx_errors(&f), 0);
    pfx_end(&f);

    (void)arm_pattern(&f, "{.., 42} -> 1");
    EK_CHECK_U64(pfx_errors(&f), 0);
    pfx_end(&f);
}

int main(void)
{
    simple_patterns();
    constructor_patterns();
    brace_patterns();
    or_patterns();
    paren_patterns();
    excluded_patterns();
    rest_edges();
    return ek_test_report("parse/pattern");
}
