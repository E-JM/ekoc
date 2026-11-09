#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli/internal.h"

#include "support/surface.h"

#include "../resolve/regex_source.h"
#include "../resolve/std_source.h"
#include "../resolve/time_source.h"

/* The tooling commands of Clause 11.3: `where` prints the path of the
   standard library or a companion module in the distribution, `doc`
   prints the signature, documentation comment, and origin of a
   source-declared member, and `doc -src` prints the definition
   itself. The text comes from the sources embedded in the compiler,
   so the answers match what the checker reads; the paths are the
   distribution layout. A member the std source does not declare is a
   compiler-supplied row (Clause 9.1.1) and reports that origin. */

/* The embedded sources, one per module the compiler ships. */
typedef struct {
    const char *path;
    char *(*join)(ek_arena *arena);
} shipped;

static const shipped SHIPPED[] = {
    {"lib/std.eko", ek_std_source_join},
    {"lib/std/regex.eko", ek_regex_source_join},
    {"lib/std/time.eko", ek_time_source_join},
    {NULL, NULL}
};

/* The distribution path of one shipped module under the compiler's
   tree root. */
static void shipped_path(char *buf, size_t cap, const char *root,
                         const char *rel)
{
    snprintf(buf, cap, "%s/%s", root, rel);
}

int cli_where(const char *root, const char *stem, const char *sub)
{
    char path[4096];
    size_t i;

    if (!stem || strcmp(stem, "std") != 0) {
        fprintf(stderr,
                "ekoc: `where` names the standard library: "
                "`ekoc where std [<stem>]`\n");
        return 2;
    }
    if (!sub) {
        shipped_path(path, sizeof path, root, "lib/std.eko");
        printf("%s\n", path);
        return 0;
    }
    for (i = 0; SHIPPED[i].path; i++) {
        const char *base = strrchr(SHIPPED[i].path, '/');
        const char *file = base ? base + 1 : SHIPPED[i].path;
        const char *dot = strrchr(file, '.');

        if (dot && strncmp(file, sub, (size_t)(dot - file)) == 0
            && strlen(sub) == (size_t)(dot - file)) {
            shipped_path(path, sizeof path, root, SHIPPED[i].path);
            printf("%s\n", path);
            return 0;
        }
    }
    fprintf(stderr, "ekoc: no companion module `%s`\n", sub);
    return 2;
}

/* True when the line declares a top-level member: a name at column
   zero followed by the annotation colon. */
static int decl_line(const char *line, const char **name, size_t *len)
{
    size_t n = 0;

    if (line[0] == '/' || line[0] == '\0' || line[0] == '\n') {
        return 0;
    }
    while (line[n] && (line[n] == '_' || line[n] == '.'
                       || (line[n] >= 'A' && line[n] <= 'Z')
                       || (line[n] >= 'a' && line[n] <= 'z')
                       || (line[n] >= '0' && line[n] <= '9'))) {
        n++;
    }
    if (n == 0) {
        return 0;
    }
    {
        size_t k = n;

        while (line[k] == ' ') {
            k++;
        }
        if (line[k] != ':') {
            return 0;
        }
    }
    *name = line;
    *len = n;
    return 1;
}

/* The embedded source of one module as an arena buffer of lines. */
static char **shipped_lines(ek_arena *arena, const shipped *s, size_t *nlines)
{
    char *text = s->join(arena);
    char **lines;
    size_t n = 0, cap = 64, i, start = 0;

    if (!text) {
        return NULL;
    }
    lines = ek_arena_alloc(arena, cap * sizeof *lines);
    if (!lines) {
        return NULL;
    }
    for (i = 0; ; i++) {
        char ch = text[i];

        if (ch == '\n' || ch == '\0') {
            if (n == cap) {
                char **grown = ek_arena_alloc(arena, cap * 2 * sizeof *lines);

                if (!grown) {
                    return NULL;
                }
                memcpy(grown, lines, n * sizeof *lines);
                lines = grown;
                cap *= 2;
            }
            text[i] = '\0';
            lines[n++] = text + start;
            if (ch == '\0') {
                break;
            }
            start = i + 1;
        }
    }
    *nlines = n;
    return lines;
}

/* The `///` comment lines above the declaration, printed verbatim. */
static void print_doc_comment(char **lines, size_t decl)
{
    size_t start = decl;

    while (start > 0 && strncmp(lines[start - 1], "///", 3) == 0) {
        start--;
    }
    for (; start < decl; start++) {
        printf("%s\n", lines[start]);
    }
}

/* The signature: the declaration line and any continuation up to the
   `=` that opens the body. */
static void print_signature(char **lines, size_t decl, size_t nlines)
{
    size_t end = decl;

    while (end < nlines) {
        const char *eq = strchr(lines[end], '=');
        const char *arrow = strstr(lines[end], "->");

        if (eq || arrow) {
            break;
        }
        end++;
    }
    for (; decl <= end && decl < nlines; decl++) {
        printf("%s\n", lines[decl]);
    }
}

/* The whole definition for -src: the declaration through the body,
   up to the next top-level line. */
static void print_source_span(char **lines, size_t decl, size_t nlines,
                              size_t from, const char *path)
{
    size_t end = decl + 1;
    const char *skip_name;
    size_t skip_len;

    printf("%s:%zu\n", path, from);
    while (end < nlines && !decl_line(lines[end], &skip_name, &skip_len)) {
        end++;
    }
    if (end > decl + 1) {
        printf("%s:%zu-%zu\n", path, from, from + (end - 1 - decl));
    }
    for (; decl < end && decl < nlines; decl++) {
        printf("%s\n", lines[decl]);
    }
}

/* The C vocabulary: no module surface and no row table stand behind
   it, so the closed member set the checker reads lists here (Clause
   10.2). */
static const char *const C_ROWS[] = {
    "C.get_errno", "C.Size.to_uint", "C.Size.of", NULL
};

/* The compiler-supplied rows of one namespace or owner. */
static int doc_compiler_row(const char *owner, const char *member)
{
    size_t i;
    int hit = 0;

    for (i = 0; C_ROWS[i]; i++) {
        const char *r = C_ROWS[i];

        if ((!owner && strcmp(r, member) == 0)
            || (owner && strlen(r) == strlen(owner) + 1 + strlen(member)
                && strncmp(r, owner, strlen(owner)) == 0
                && r[strlen(owner)] == '.'
                && strcmp(r + strlen(owner) + 1, member) == 0)) {
            printf("%s - compiler-supplied row (Clause 9.1.1)\n", r);
            hit = 1;
        }
    }
    for (i = 0; i < ek_surface_rows_len; i++) {
        const ek_surface_row *row = &ek_surface_rows[i];

        if (!row->name || !row->ns) {
            continue;
        }
        if ((owner && strcmp(row->ns, owner) == 0
             && strcmp(row->name, member) == 0)
            || (!owner && strcmp(row->ns, member) == 0)) {
            printf("%s.%s - compiler-supplied row (Clause 9.1.1)\n",
                   row->ns, row->name);
            hit = 1;
        }
    }
    return hit;
}

/* The members one type or namespace declares in the shipped sources. */
static int doc_members(ek_arena *arena, const char *owner)
{
    size_t s, i, hit = 0;

    for (s = 0; SHIPPED[s].path; s++) {
        size_t nlines = 0;
        char **lines = shipped_lines(arena, &SHIPPED[s], &nlines);

        if (!lines) {
            continue;
        }
        for (i = 0; i < nlines; i++) {
            const char *name;
            size_t len;

            if (!decl_line(lines[i], &name, &len)
                || len <= strlen(owner) + 1
                || strncmp(name, owner, strlen(owner)) != 0
                || name[strlen(owner)] != '.') {
                continue;
            }
            print_doc_comment(lines, i);
            print_signature(lines, i, nlines);
            printf("  %s:%zu\n",
                   SHIPPED[s].path, i + 1);
            hit = 1;
        }
    }
    return hit;
}

int cli_doc(const char *root, int src, const char *name)
{
    ek_arena *arena = ek_arena_create();
    const char *dot = strchr(name, '.');
    const char *owner = dot ? NULL : name;
    char owner_buf[256];
    const char *member = name;
    size_t s, i;
    int hit = 0;

    if (!arena) {
        fprintf(stderr, "ekoc: out of memory\n");
        return 2;
    }
    if (dot) {
        size_t n = (size_t)(dot - name);

        if (n >= sizeof owner_buf) {
            ek_arena_destroy(arena);
            return 2;
        }
        memcpy(owner_buf, name, n);
        owner_buf[n] = '\0';
        owner = owner_buf;
        member = dot + 1;
    }
    for (s = 0; SHIPPED[s].path; s++) {
        size_t nlines = 0;
        char **lines = shipped_lines(arena, &SHIPPED[s], &nlines);
        char path[4096];

        if (!lines) {
            continue;
        }
        shipped_path(path, sizeof path, root, SHIPPED[s].path);
        for (i = 0; i < nlines; i++) {
            const char *dname;
            size_t len;

            if (!decl_line(lines[i], &dname, &len)) {
                continue;
            }
            if (strlen(name) != len
                || strncmp(dname, name, len) != 0) {
                continue;
            }
            if (src) {
                print_source_span(lines, i, nlines, i + 1, path);
            } else {
                print_doc_comment(lines, i);
                print_signature(lines, i, nlines);
                printf("  %s:%zu\n", SHIPPED[s].path, i + 1);
            }
            hit = 1;
        }
    }
    /* A compiler-supplied row reports its compiler origin rather
       than a file path and span (Clause 11.3). */
    if (!hit) {
        hit = doc_compiler_row(owner, member);
    }
    if (!hit && !src && !dot) {
        /* A type or namespace: list the members it declares, from the
           shipped sources and from the compiler rows. */
        hit = doc_members(arena, name);
        hit = doc_compiler_row(NULL, name) != 0 || hit;
    }
    if (!hit && !src && !dot && strcmp(name, "std") == 0) {
        /* The module stem: the distribution path of its source. */
        char path[4096];

        shipped_path(path, sizeof path, root, "lib/std.eko");
        printf("%s\n", path);
        hit = 1;
    }
    ek_arena_destroy(arena);
    if (!hit) {
        fprintf(stderr, "ekoc: no member `%s` in the shipped sources\n",
                name);
        return 2;
    }
    return 0;
}
