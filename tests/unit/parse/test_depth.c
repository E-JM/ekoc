#include "common.h"

#include <stdio.h>
#include <stdlib.h>

/* The height guard: deep but real nesting parses, runaway nesting
   recovers with one E-nesting-depth and terminates. The sources here
   sit near the guard's two faces -- just under the ceiling and well
   past it -- for the constructs whose parsers nest with the input:
   parenthesized expressions, operator chains, member chains, bracket
   types, brace patterns, constructor application, lambda parameter
   groups, and nested composites. The ceiling itself is the parser's
   private constant; the under cases below sit at half of it, the over
   cases at double or more. */

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


static void run(parse_fixture *f, char *src, int under);

/* A left-leaning operator chain: each `+` adds one tree level. */
static char *chain_source(size_t terms)
{
    char *s = malloc(terms * 2 + 32);
    size_t i;

    if (!s) {
        return NULL;
    }
    strcpy(s, "_t: Int = ");
    for (i = 0; i < terms; i++) {
        strcat(s, i == 0 ? "1" : "+1");
    }
    strcat(s, "\n");
    return s;
}

/* A left-leaning member chain: each `.b` adds one tree level. */
static char *member_source(size_t links)
{
    char *s = malloc(links * 2 + 32);
    size_t i;

    if (!s) {
        return NULL;
    }
    strcpy(s, "_t: Int = a");
    for (i = 0; i < links; i++) {
        strcat(s, ".b");
    }
    strcat(s, "\n");
    return s;
}

/* Nested composite literals: each `{a: ` adds one tree level. */
static char *composite_source(size_t n)
{
    char *s = malloc(n * 8 + 32);
    size_t i;

    if (!s) {
        return NULL;
    }
    strcpy(s, "_t: Int = ");
    for (i = 0; i < n; i++) {
        strcat(s, "{a: ");
    }
    strcat(s, "1");
    for (i = 0; i < n; i++) {
        strcat(s, "}");
    }
    strcat(s, "\n");
    return s;
}

/* Flat forms are lists, not nesting: they must parse at any length. */
static char *args_source(size_t n)
{
    char *s = malloc(n * 2 + 32);
    size_t i;

    if (!s) {
        return NULL;
    }
    strcpy(s, "_t: Int = f(");
    for (i = 0; i < n; i++) {
        strcat(s, i == 0 ? "1" : ",1");
    }
    strcat(s, ")\n");
    return s;
}

/* The chain at the ceiling's edge parses; its tree height, including
   the binding wrapper, is EK_PARSE_MAX_DEPTH - 1. */
static void chains(void)
{
    parse_fixture f;

    run(&f, chain_source(499), 1);
    run(&f, chain_source(999), 1);
    run(&f, chain_source(60000), 0);
}

static void member_chains(void)
{
    parse_fixture f;

    /* The head name is one level, so the binding at the edge holds
       EK_PARSE_MAX_DEPTH - 2 links. */
    run(&f, member_source(998), 1);
    run(&f, member_source(60000), 0);
}

static void composites(void)
{
    parse_fixture f;

    run(&f, composite_source(400), 1);
    run(&f, composite_source(2000), 0);
}

static void flat_lists(void)
{
    parse_fixture f;

    run(&f, args_source(20000), 1);
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
        /* The ceiling is fatal to this parse: one diagnostic, then
           recovery drains the rest of the stream in silence. */
        EK_CHECK_U64(pfx_errors(f), 1);
        pfx_collect(f);
        EK_CHECK(pfx_has_code(f, "E-nesting-depth"));
    }
    pfx_end(f);
    free(src);
}

static void parens(void)
{
    parse_fixture f;

    run(&f, paren_source(500), 1);
    run(&f, paren_source(2000), 0);
    run(&f, paren_source(50000), 0);
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

/* A fatal depth error quiets recovery: the member before it survives,
   the rejected member and everything after it are drained silently. */
static void fatal_depth_quiets(void)
{
    parse_fixture f;
    char *deep = paren_source(2000);
    char *s;

    if (!deep) {
        EK_CHECK(0);
        return;
    }
    s = malloc(strlen(deep) + 64);
    if (!s) {
        free(deep);
        EK_CHECK(0);
        return;
    }
    sprintf(s, "v1: Int = 1\n%sv2: Int = 2\n", deep);
    pfx_begin(&f, s);
    EK_CHECK_U64(pfx_errors(&f), 1);
    pfx_collect(&f);
    EK_CHECK(pfx_has_code(&f, "E-nesting-depth"));
    EK_CHECK(f.ast.module != NULL);
    EK_CHECK_U64(f.ast.module->u.kids.count, 1);
    EK_CHECK_U64(f.ast.module->u.kids.data[0]->kind, EK_N_LET);
    pfx_end(&f);
    free(deep);
    free(s);
}

int main(void)
{
    parens();
    chains();
    member_chains();
    composites();
    list_types();
    brace_patterns();
    applies();
    lambda_groups();
    flat_lists();
    fatal_depth_quiets();
    return ek_test_report("parse/depth");
}
