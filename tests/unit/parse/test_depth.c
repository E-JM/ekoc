#include "common.h"

#include <stdio.h>
#include <stdlib.h>

/* The recursion guard: deep but real nesting parses, runaway nesting
   recovers with E-nesting-depth and terminates. The sources here sit
   near the guard's two faces -- just under the ceiling and well past
   it -- for the constructs whose parsers nest with the input:
   parenthesized expressions, bracket types, brace patterns,
   constructor application, and lambda parameter groups. The ceiling
   itself is the parser's private constant; the under cases below sit
   at half of it, the over cases at double. */

/* Builds "open" x n + seed + "close" x n in a fresh buffer. */
static char *nested(size_t n, const char *open, const char *seed,
                    const char *close)
{
    size_t lo = strlen(open), ls = strlen(seed), lc = strlen(close);
    char *s = malloc(n * (lo + lc) + ls + 64);

    if (!s) {
        return NULL;
    }
    s[0] = '\0';
    for (size_t i = 0; i < n; i++) {
        strcat(s, open);
    }
    strcat(s, seed);
    for (size_t i = 0; i < n; i++) {
        strcat(s, close);
    }
    return s;
}

/* A binding line around a deep parenthesized expression. */
static char *paren_source(size_t n)
{
    char *body = nested(n, "(", "1", ")");
    char *s;

    if (!body) {
        return NULL;
    }
    s = malloc(strlen(body) + 32);
    if (s) {
        sprintf(s, "_t: Int = %s\n", body);
    }
    free(body);
    return s;
}

/* An annotation of a deeply nested list type: each `[]` is one
   bracket_type recursion through p_type. */
static char *type_source(size_t n)
{
    char *body = nested(n, "[]", "Int", "");
    char *s;

    if (!body) {
        return NULL;
    }
    s = malloc(strlen(body) + 32);
    if (s) {
        sprintf(s, "_t: %s = _\n", body);
    }
    free(body);
    return s;
}

/* A case arm whose pattern nests brace groups. */
static char *pattern_source(size_t n)
{
    char *body = nested(n, "{a: ", "1", "}");
    char *s;

    if (!body) {
        return NULL;
    }
    s = malloc(strlen(body) + 64);
    if (s) {
        sprintf(s, "_t: Int = case x:\n            %s -> 0\n"
                   "            _ -> 1\n", body);
    }
    free(body);
    return s;
}

/* A run of juxtapped constructors: one nesting level per token. */
static char *apply_source(size_t n)
{
    char *s = malloc(n * 2 + 32);

    if (!s) {
        return NULL;
    }
    s[0] = '\0';
    strcat(s, "_t: Int = ");
    for (size_t i = 0; i < n; i++) {
        strcat(s, "W ");
    }
    strcat(s, "1\n");
    return s;
}

/* A lambda whose parameter groups nest. */
static char *lambda_source(size_t n)
{
    char *body = nested(n, "(", "a", ")");
    char *s;

    if (!body) {
        return NULL;
    }
    s = malloc(strlen(body) + 32);
    if (s) {
        sprintf(s, "_t: Int = %s: a\n", body);
    }
    free(body);
    return s;
}

static void run(parse_fixture *f, char *src, int under)
{
    if (!src) {
        EK_CHECK(0);
        return;
    }
    pfx_begin(f, src);
    if (under) {
        EK_CHECK_U64(pfx_errors(f), 0);
    } else {
        /* The ceiling is diagnosed once; the rejected member's leftover
           brackets recover per position, so the run's total stays
           linear in the stream, never per-level at the guard. */
        EK_CHECK(pfx_errors(f) > 0);
        pfx_collect(f);
        EK_CHECK(pfx_has_code(f, "E-nesting-depth"));
        EK_CHECK(f->dn <= f->lay.count + 8);
    }
    pfx_end(f);
    free(src);
}

static void parens(void)
{
    parse_fixture f;

    run(&f, paren_source(500), 1);
    run(&f, paren_source(2000), 0);
}

static void list_types(void)
{
    parse_fixture f;

    run(&f, type_source(500), 1);
    run(&f, type_source(2000), 0);
}

static void brace_patterns(void)
{
    parse_fixture f;

    run(&f, pattern_source(400), 1);
    run(&f, pattern_source(2000), 0);
}

static void applies(void)
{
    parse_fixture f;

    run(&f, apply_source(500), 1);
    run(&f, apply_source(2000), 0);
}

static void lambda_groups(void)
{
    parse_fixture f;

    run(&f, lambda_source(500), 1);
    run(&f, lambda_source(2000), 0);
}

/* The recovery leaves a usable stream: after the diagnostic the
   parser resyncs, so the member after the rejected one is present. */
static void recovery_keeps_members(void)
{
    parse_fixture f;
    char *deep = paren_source(2000);
    char *s;
    const ek_node *last;

    if (!deep) {
        EK_CHECK(0);
        return;
    }
    s = malloc(strlen(deep) + 32);
    if (!s) {
        free(deep);
        EK_CHECK(0);
        return;
    }
    sprintf(s, "%sv2: Int = 2\n", deep);
    pfx_begin(&f, s);
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_collect(&f);
    EK_CHECK(pfx_has_code(&f, "E-nesting-depth"));
    EK_CHECK(f.ast.module != NULL);
    EK_CHECK(f.ast.module->u.kids.count >= 1);
    last = f.ast.module->u.kids.data[f.ast.module->u.kids.count - 1];
    EK_CHECK_U64(last->kind, EK_N_LET);
    EK_CHECK_U64(last->u.bind.pattern->kind, EK_N_P_VAR);
    EK_CHECK_STR(last->u.bind.pattern->u.path.data[0], "v2");
    pfx_end(&f);
    free(deep);
    free(s);
}

int main(void)
{
    parens();
    list_types();
    brace_patterns();
    applies();
    lambda_groups();
    recovery_keeps_members();
    return ek_test_report("parse/depth");
}
