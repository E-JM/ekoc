#include "common.h"

/* case and if (Clause 6.7): arms, guards, or-patterns, multiple
   scrutinees, guards, the single-line form, and else reattachment. */

static const char *block_case =
    "f: (k: Key) -> String =\n"
    "    case k:\n"
    "        Up | Down -> \"vertical\"\n"
    "        Left -> \"left\"\n"
    "        _ -> \"other\"\n";

static const char *guarded_case =
    "f: (n: Int) -> String =\n"
    "    case n:\n"
    "        n if n < 0 -> \"negative\"\n"
    "        _ -> \"other\"\n";

static const char *multi_case =
    "f: (n: Int) -> String =\n"
    "    case n % 3 == 0, n % 5 == 0:\n"
    "        True, True -> \"FizzBuzz\"\n"
    "        _, _ -> show(n)\n";

static void case_arms(void)
{
    parse_fixture f;
    const ek_node *ce;

    pfx_begin(&f, block_case);
    EK_CHECK_U64(pfx_errors(&f), 0);
    ce = pfx_find(f.ast.module, EK_N_CASE);
    EK_CHECK(ce != NULL);
    EK_CHECK_U64(ce->u.caseexpr.scrutinees.count, 1);
    EK_CHECK_U64(ce->u.caseexpr.arms.count, 3);
    EK_CHECK_U64(ce->u.caseexpr.arms.data[0]->u.arm.patterns.count, 1);
    EK_CHECK_U64(ce->u.caseexpr.arms.data[0]->u.arm.patterns.data[0]->kind,
                 EK_N_P_OR);
    EK_CHECK_U64(ce->u.caseexpr.arms.data[0]->u.arm.guard, NULL);
    EK_CHECK_U64(ce->u.caseexpr.arms.data[2]->u.arm.patterns.data[0]->kind,
                 EK_N_P_WILDCARD);
    pfx_end(&f);

    pfx_begin(&f, guarded_case);
    EK_CHECK_U64(pfx_errors(&f), 0);
    ce = pfx_find(f.ast.module, EK_N_CASE);
    EK_CHECK(ce != NULL);
    EK_CHECK_U64(ce->u.caseexpr.arms.data[0]->u.arm.guard->kind, EK_N_BINARY);
    EK_CHECK_U64(ce->u.caseexpr.arms.data[1]->u.arm.guard, NULL);
    pfx_end(&f);

    /* Multiple scrutinees, one pattern each (Clause 6.7.2). */
    pfx_begin(&f, multi_case);
    EK_CHECK_U64(pfx_errors(&f), 0);
    ce = pfx_find(f.ast.module, EK_N_CASE);
    EK_CHECK(ce != NULL);
    EK_CHECK_U64(ce->u.caseexpr.scrutinees.count, 2);
    EK_CHECK_U64(ce->u.caseexpr.arms.data[0]->u.arm.patterns.count, 2);
    pfx_end(&f);
}

static void single_line_case(void)
{
    parse_fixture f;
    const ek_node *ce;

    /* `;` separates arms on one line (Clause 6.7.5). */
    pfx_begin(&f, "f: Int =\n    case r: Ok v -> v; Err e -> 0\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    ce = pfx_find(f.ast.module, EK_N_CASE);
    EK_CHECK(ce != NULL);
    EK_CHECK_U64(ce->u.caseexpr.arms.count, 2);
    pfx_end(&f);

    pfx_begin(&f, "f: Int =\n    case n % 2: 0 -> \"even\"; _ -> \"odd\"\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    ce = pfx_find(f.ast.module, EK_N_CASE);
    EK_CHECK(ce != NULL);
    EK_CHECK_U64(ce->u.caseexpr.arms.count, 2);
    pfx_end(&f);
}

static void if_forms(void)
{
    parse_fixture f;
    const ek_node *ie;

    /* Block branches of Clause 6.7.4. */
    pfx_begin(&f,
        "f: (n: Int) -> Int =\n"
        "    if n < 0:\n"
        "        0 - n\n"
        "    else:\n"
        "        n\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    ie = pfx_find(f.ast.module, EK_N_IF);
    EK_CHECK(ie != NULL);
    EK_CHECK_U64(ie->u.ifexpr.cond->kind, EK_N_BINARY);
    EK_CHECK_U64(ie->u.ifexpr.then->kind, EK_N_BLOCK);
    EK_CHECK_U64(ie->u.ifexpr.els->kind, EK_N_BLOCK);
    pfx_end(&f);

    /* The inline single-line form with a `;` suite. */
    pfx_begin(&f, "f: Int =\n    x = if c: a(); b() else: z()\n    x\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    ie = pfx_find(f.ast.module, EK_N_IF);
    EK_CHECK(ie != NULL);
    EK_CHECK_U64(ie->u.ifexpr.then->kind, EK_N_BLOCK);
    EK_CHECK_U64(ie->u.ifexpr.then->u.kids.count, 2);
    pfx_end(&f);

    /* An omitted else stays NULL (Clause 6.7.4 sugar). */
    pfx_begin(&f,
        "f: (n: Int) -> Int =\n"
        "    if err_count > 0:\n"
        "        1\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    ie = pfx_find(f.ast.module, EK_N_IF);
    EK_CHECK(ie != NULL);
    EK_CHECK_U64(ie->u.ifexpr.els, NULL);
    pfx_end(&f);

    /* Inline then and else on one line (6.2.3). */
    pfx_begin(&f, "f: Int =\n    x = if n < 0: 0 else: n\n    x\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    ie = pfx_find(f.ast.module, EK_N_IF);
    EK_CHECK(ie != NULL);
    EK_CHECK_U64(ie->u.ifexpr.then->kind, EK_N_INT_LIT);
    EK_CHECK_U64(ie->u.ifexpr.els->kind, EK_N_NAME);
    pfx_end(&f);
}

static void else_reattachment(void)
{
    parse_fixture f;
    const ek_node *ie;

    /* An else line left of the then block reattaches across the
       DEDENTs the layout emitted (A.4 rule 8). */
    pfx_begin(&f,
        "f: (c: Bool) -> Int =\n"
        "    if c:\n"
        "        1\n"
        "else:\n"
        "        2\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    ie = pfx_find(f.ast.module, EK_N_IF);
    EK_CHECK(ie != NULL);
    EK_CHECK(ie->u.ifexpr.els != NULL);
    pfx_end(&f);

    /* An if inside a case arm body (geolib_use._kind shape). */
    pfx_begin(&f,
        "f: (w: Int, h: Int) -> String =\n"
        "    case w:\n"
        "        0 -> if w > h: \"wide\" else: \"tall\"\n"
        "        _ -> \"other\"\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK(pfx_find(f.ast.module, EK_N_IF) != NULL);
    EK_CHECK_U64(pfx_count_kind(f.ast.module, EK_N_ARM), 2);
    pfx_end(&f);
}

static void case_as_value(void)
{
    parse_fixture f;
    const ek_node *let;

    /* A case is the whole right-hand side of a binding (6.5.1). */
    pfx_begin(&f,
        "f: (n: Int) -> String =\n"
        "    msg: String =\n"
        "        case n:\n"
        "            0 -> \"zero\"\n"
        "            _ -> \"nonzero\"\n"
        "    msg\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    let = pfx_find(f.ast.module, EK_N_LET);
    EK_CHECK(let != NULL);
    EK_CHECK_U64(let->u.bind.value->kind, EK_N_BLOCK);
    EK_CHECK(pfx_find(f.ast.module, EK_N_CASE) != NULL);
    EK_CHECK_U64(pfx_count_kind(f.ast.module, EK_N_ARM), 2);
    pfx_end(&f);
}

int main(void)
{
    case_arms();
    single_line_case();
    if_forms();
    else_reattachment();
    case_as_value();
    return ek_test_report("parse/ctl");
}
