#include "common.h"

/* case and if: arms, or-patterns, multiple scrutinees, guards, the
   single-line form, and else reattachment. */

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

    /* Multiple scrutinees, one pattern each. */
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

    /* `;` separates arms on one line. */
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

    /* Block branches. */
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

    /* An omitted else stays NULL. */
    pfx_begin(&f,
        "f: (n: Int) -> Int =\n"
        "    if err_count > 0:\n"
        "        1\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    ie = pfx_find(f.ast.module, EK_N_IF);
    EK_CHECK(ie != NULL);
    EK_CHECK_U64(ie->u.ifexpr.els, NULL);
    pfx_end(&f);

    /* Inline then and else on one line. */
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
       DEDENTs the layout emitted. */
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

    /* A case is the whole right-hand side of a binding. */
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

/* The shape that motivates arm body blocks: a binding and the value
   share the arm body, both one level under the arm line. */
static void case_arm_body_block(void)
{
    parse_fixture f;
    const ek_node *ce;
    const ek_node *body;

    pfx_begin(&f,
        "route: (store: Store, cmd: String) -> Result Outcome String =\n"
        "    case cmd:\n"
        "        \"list\" ->\n"
        "            lines: []String = list_lines(store)\n"
        "            Ok {store: None, lines: lines}\n"
        "        _ -> Err \"unknown\"\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    ce = pfx_find(f.ast.module, EK_N_CASE);
    EK_CHECK(ce != NULL);
    EK_CHECK_U64(ce->u.caseexpr.arms.count, 2);
    body = ce->u.caseexpr.arms.data[0]->u.arm.body;
    EK_CHECK_U64(body->kind, EK_N_BLOCK);
    EK_CHECK_U64(body->u.kids.count, 2);
    EK_CHECK_U64(body->u.kids.data[0]->kind, EK_N_LET);
    EK_CHECK_U64(body->u.kids.data[0]->u.bind.type->kind, EK_N_T_LIST);
    EK_CHECK_U64(body->u.kids.data[1]->kind, EK_N_APPLY);
    /* The arm with an inline body keeps a bare expression. */
    EK_CHECK_U64(ce->u.caseexpr.arms.data[1]->u.arm.body->kind, EK_N_APPLY);
    pfx_end(&f);
}

/* An arm body block holds ordinary members, a rebinding included. */
static void case_arm_body_members(void)
{
    parse_fixture f;
    const ek_node *ce;
    const ek_node *body;

    pfx_begin(&f,
        "f: (s: String) -> Int =\n"
        "    case s:\n"
        "        \"a\" ->\n"
        "            n := 0\n"
        "            n\n"
        "        _ -> 1\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    ce = pfx_find(f.ast.module, EK_N_CASE);
    EK_CHECK(ce != NULL);
    body = ce->u.caseexpr.arms.data[0]->u.arm.body;
    EK_CHECK_U64(body->kind, EK_N_BLOCK);
    EK_CHECK_U64(body->u.kids.count, 2);
    EK_CHECK_U64(body->u.kids.data[0]->kind, EK_N_REBIND);
    EK_CHECK_U64(body->u.kids.data[1]->kind, EK_N_NAME);
    pfx_end(&f);
}

/* A case nested in an arm body block: the inner arms open their own
   body blocks, and the outer arm stays one block with one member. */
static void case_arm_body_nested_case(void)
{
    parse_fixture f;
    const ek_node *outer;
    const ek_node *inner;
    const ek_node *body;

    pfx_begin(&f,
        "f: (a: String, b: String) -> Int =\n"
        "    case a:\n"
        "        \"x\" ->\n"
        "            case b:\n"
        "                \"y\" ->\n"
        "                    1\n"
        "                _ -> 2\n"
        "        _ -> 3\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(pfx_count_kind(f.ast.module, EK_N_CASE), 2);
    outer = pfx_find(f.ast.module, EK_N_CASE);
    EK_CHECK(outer != NULL);
    EK_CHECK_U64(outer->u.caseexpr.arms.count, 2);
    body = outer->u.caseexpr.arms.data[0]->u.arm.body;
    EK_CHECK_U64(body->kind, EK_N_BLOCK);
    EK_CHECK_U64(body->u.kids.count, 1);
    inner = body->u.kids.data[0];
    EK_CHECK_U64(inner->kind, EK_N_CASE);
    EK_CHECK_U64(inner->u.caseexpr.arms.count, 2);
    EK_CHECK_U64(inner->u.caseexpr.arms.data[0]->u.arm.body->kind,
                 EK_N_BLOCK);
    EK_CHECK_U64(inner->u.caseexpr.arms.data[1]->u.arm.body->kind,
                 EK_N_INT_LIT);
    pfx_end(&f);
}

/* An if as the first member of an arm body block: its branches are
   blocks of their own and the else reattaches inside the arm block. */
static void case_arm_body_if_else(void)
{
    parse_fixture f;
    const ek_node *ce;
    const ek_node *body;
    const ek_node *ie;

    pfx_begin(&f,
        "f: (n: Int) -> Int =\n"
        "    case n:\n"
        "        0 ->\n"
        "            if n < 0:\n"
        "                1\n"
        "            else:\n"
        "                2\n"
        "        _ -> 3\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    ce = pfx_find(f.ast.module, EK_N_CASE);
    EK_CHECK(ce != NULL);
    EK_CHECK_U64(ce->u.caseexpr.arms.count, 2);
    body = ce->u.caseexpr.arms.data[0]->u.arm.body;
    EK_CHECK_U64(body->kind, EK_N_BLOCK);
    EK_CHECK_U64(body->u.kids.count, 1);
    ie = body->u.kids.data[0];
    EK_CHECK_U64(ie->kind, EK_N_IF);
    EK_CHECK_U64(ie->u.ifexpr.then->kind, EK_N_BLOCK);
    EK_CHECK_U64(ie->u.ifexpr.els->kind, EK_N_BLOCK);
    EK_CHECK_U64(ce->u.caseexpr.arms.data[1]->u.arm.body->kind,
                 EK_N_INT_LIT);
    pfx_end(&f);
}

/* Guards, or-patterns, and several scrutinees keep their shape when
   the arm body is a block. */
static void case_arm_body_guarded(void)
{
    parse_fixture f;
    const ek_node *ce;

    pfx_begin(&f,
        "f: (k: Key, c: Bool) -> Int =\n"
        "    case k:\n"
        "        Up | Down if c ->\n"
        "            1\n"
        "        _ -> 2\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    ce = pfx_find(f.ast.module, EK_N_CASE);
    EK_CHECK(ce != NULL);
    EK_CHECK_U64(ce->u.caseexpr.arms.data[0]->u.arm.patterns.data[0]->kind,
                 EK_N_P_OR);
    EK_CHECK(ce->u.caseexpr.arms.data[0]->u.arm.guard != NULL);
    EK_CHECK_U64(ce->u.caseexpr.arms.data[0]->u.arm.body->kind, EK_N_BLOCK);
    pfx_end(&f);

    pfx_begin(&f,
        "f: (a: Bool, b: Bool) -> Int =\n"
        "    case a, b:\n"
        "        True, True ->\n"
        "            1\n"
        "        _, _ -> 2\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    ce = pfx_find(f.ast.module, EK_N_CASE);
    EK_CHECK(ce != NULL);
    EK_CHECK_U64(ce->u.caseexpr.scrutinees.count, 2);
    EK_CHECK_U64(ce->u.caseexpr.arms.data[0]->u.arm.patterns.count, 2);
    EK_CHECK_U64(ce->u.caseexpr.arms.data[0]->u.arm.body->kind, EK_N_BLOCK);
    pfx_end(&f);
}

static void arm_corners(void)
{
    parse_fixture f;
    const ek_node *ce;

    /* Three or-alternatives flatten into one arm. */
    pfx_begin(&f,
        "f: (k: Key) -> String =\n"
        "    case k:\n"
        "        Up | Down | Left -> \"move\"\n"
        "        _ -> \"other\"\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    ce = pfx_find(f.ast.module, EK_N_CASE);
    EK_CHECK(ce != NULL);
    EK_CHECK_U64(ce->u.caseexpr.arms.data[0]->u.arm.patterns.data[0]->kind,
                 EK_N_P_OR);
    EK_CHECK_U64(ce->u.caseexpr.arms.data[0]->u.arm.patterns.data[0]->u.kids.count,
                 3);
    pfx_end(&f);

    /* A guard over multiple scrutinees. */
    pfx_begin(&f,
        "f: (a: Bool, b: Bool) -> String =\n"
        "    case a, b:\n"
        "        True, False if go -> \"go\"\n"
        "        _, _ -> \"stay\"\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    ce = pfx_find(f.ast.module, EK_N_CASE);
    EK_CHECK(ce != NULL);
    EK_CHECK_U64(ce->u.caseexpr.arms.data[0]->u.arm.patterns.count, 2);
    EK_CHECK(ce->u.caseexpr.arms.data[0]->u.arm.guard != NULL);
    pfx_end(&f);

    /* `;` separates arms inside an indented block, too. */
    pfx_begin(&f,
        "f: (r: Result Int String) -> Int =\n"
        "    case r:\n"
        "        Ok v -> v; Err e -> e.length()\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    ce = pfx_find(f.ast.module, EK_N_CASE);
    EK_CHECK(ce != NULL);
    EK_CHECK_U64(ce->u.caseexpr.arms.count, 2);
    pfx_end(&f);
}

static void else_chains(void)
{
    parse_fixture f;
    const ek_node *ie;

    /* `else` nests through the suite. */
    pfx_begin(&f, "f: Int =\n    if a: 1 else: if b: 2 else: 3\n    0\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    ie = pfx_find(f.ast.module, EK_N_IF);
    EK_CHECK(ie != NULL);
    EK_CHECK_U64(ie->u.ifexpr.els->kind, EK_N_IF);
    EK_CHECK_U64(ie->u.ifexpr.els->u.ifexpr.els->kind, EK_N_INT_LIT);
    pfx_end(&f);

    /* An `else` guarding nothing is diagnosed. */
    pfx_begin(&f, "f: Int =\n    if a: 1 else if b: 2\n    0\n");
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_collect(&f);
    EK_CHECK(pfx_msg_has(&f, "6.7.4"));
    pfx_end(&f);
}

int main(void)
{
    case_arms();
    single_line_case();
    if_forms();
    else_reattachment();
    case_as_value();
    case_arm_body_block();
    case_arm_body_members();
    case_arm_body_nested_case();
    case_arm_body_if_else();
    case_arm_body_guarded();
    arm_corners();
    else_chains();
    return ek_test_report("parse/ctl");
}
