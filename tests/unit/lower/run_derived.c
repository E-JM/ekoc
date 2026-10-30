#define _POSIX_C_SOURCE 200809L

#include "common.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* The executable gate for the derived services: each program lowers,
   compiles against the runtime archive, and runs in a forked child
   whose standard output must match the pinned text exactly. The forms
   follow the standard's sections: the record, list, dictionary, and
   constructor renderings, the two display channels, the structural
   equality and the String order, with, and the JSON mapping. The suite
   assumes the repository root as its working directory. */

static int write_unit(const char *path, const char *text, size_t len)
{
    FILE *fp = fopen(path, "wb");

    if (!fp) {
        return 0;
    }
    if (fwrite(text, 1, len, fp) != len) {
        fclose(fp);
        return 0;
    }
    return fclose(fp) == 0;
}

static int compile_unit(const char *name)
{
    char cmd[256];
    int n;

    n = snprintf(cmd, sizeof cmd,
                 "cc -std=c99 -O2 -Irt build/%s.c build/libekort.a"
                 " -lm -o build/%s", name, name);
    if (n < 0 || (size_t)n >= sizeof cmd) {
        return 0;
    }
    return system(cmd) == 0;
}

/* Drains one pipe into the buffer, NUL-terminated and capped. */
static void read_pipe(int fd, char *buf, size_t cap)
{
    size_t len = 0;
    ssize_t got;

    while (len + 1 < cap) {
        got = read(fd, buf + len, cap - 1 - len);
        if (got <= 0) {
            break;
        }
        len += (size_t)got;
    }
    buf[len] = '\0';
}

/* Runs the binary in a child with both pipes captured. */
static int run_capture(const char *path, char *out, size_t outcap,
                       char *err, size_t errcap)
{
    int fds[2], efs[2];
    pid_t pid;
    int status = -1;

    out[0] = '\0';
    err[0] = '\0';
    if (pipe(fds) != 0 || pipe(efs) != 0) {
        return -1;
    }
    pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        close(efs[0]);
        close(efs[1]);
        return -1;
    }
    if (pid == 0) {
        close(fds[0]);
        close(efs[0]);
        if (dup2(fds[1], STDOUT_FILENO) < 0
            || dup2(efs[1], STDERR_FILENO) < 0) {
            _exit(127);
        }
        close(fds[1]);
        close(efs[1]);
        execl(path, path, (char *)NULL);
        _exit(127);
    }
    close(fds[1]);
    close(efs[1]);
    read_pipe(fds[0], out, outcap);
    read_pipe(efs[0], err, errcap);
    close(fds[0]);
    close(efs[0]);
    waitpid(pid, &status, 0);
    return status;
}

/* Lowers, writes, compiles, and runs one printing program; the
   captured standard output must equal want exactly. */
static void gate(const char *src, const char *name, const char *want)
{
    lfx f;
    char path[64], out[65536], err[256];
    int status;

    lfx_begin(&f, src);
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(f.low.text != NULL);
    snprintf(path, sizeof path, "build/%s.c", name);
    EK_CHECK(write_unit(path, f.low.text, f.low.len));
    lfx_end(&f);
    EK_CHECK(compile_unit(name));
    snprintf(path, sizeof path, "build/%s", name);
    status = run_capture(path, out, sizeof out, err, sizeof err);
    EK_CHECK(status >= 0 && WIFEXITED(status));
    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, want);
    EK_CHECK(err[0] == '\0');
}

/* The expected text of the deep chain: one Link and a space per level
   over the terminating End. */
static char *deep_expect(size_t depth)
{
    char *want = malloc(strlen("Link ") * depth + strlen("End\n") + 1);
    size_t pos = 0;

    if (!want) {
        return NULL;
    }
    for (size_t i = 0; i < depth; i++) {
        strcpy(want + pos, "Link ");
        pos += strlen("Link ");
    }
    strcpy(want + pos, "End\n");
    return want;
}

/* The record renders in declaration order, the nested String keeping
   its quotes; the display channel of a composite is its structural
   form. */
static void record_show(void)
{
    gate("Money: type = [cents: Int, cur: String]\n"
         "main: (o: Out) ! =\n"
         "    m: Money = {cents: 12500, cur: \"USD\"}\n"
         "    o.println(m.show())\n"
         "    _\n",
         "derived_rec",
         "{cents: 12500, cur: \"USD\"}\n");
}

/* The dictionary renders in insertion order with quoted keys, the list
   in braces. */
static void nested_show(void)
{
    gate("main: (o: Out) ! =\n"
         "    d: [String][]Int = {\"a\": {1, 2}}\n"
         "    o.println(d.show())\n"
         "    _\n",
         "derived_nested",
         "{\"a\": {1, 2}}\n");
}

/* An algebraic value renders as the constructor application, the
   payload carrying its own form. */
static void adt_show(void)
{
    gate("main: (o: Out) ! =\n"
         "    x: Option Int = Some 3\n"
         "    n: Option Int = None\n"
         "    e: Result Int String = Err \"x\"\n"
         "    o.println(x.show())\n"
         "    o.println(n.show())\n"
         "    o.println(e.show())\n"
         "    _\n",
         "derived_adt",
         "Some 3\n"
         "None\n"
         "Err \"x\"\n");
}

/* The two channels differ only at a top-level String: show bare,
   inspect quoted; inside a composite the quotes stand on both. */
static void inspect_vs_show(void)
{
    gate("main: (o: Out) ! =\n"
         "    r: [name: String] = {name: \"wd\"}\n"
         "    o.println(r.show())\n"
         "    o.println(r.inspect())\n"
         "    o.println(\"wd\".show())\n"
         "    o.println(\"wd\".inspect())\n"
         "    _\n",
         "derived_inspect",
         "{name: \"wd\"}\n"
         "{name: \"wd\"}\n"
         "wd\n"
         "\"wd\"\n");
}

/* Equality over a composite arrives as the derived member: two
   separately built trees compare structurally. */
static void tree_equality(void)
{
    gate("Tree: type = Leaf Int | Fork Tree Tree\n"
         "main: (o: Out) ! =\n"
         "    a: Tree = Leaf 1\n"
         "    b: Tree = Leaf 2\n"
         "    t1: Tree = Fork a b\n"
         "    t2: Tree = Fork (Leaf 1) (Leaf 2)\n"
         "    t3: Tree = Fork b a\n"
         "    o.println(t1.eq(t2).show())\n"
         "    o.println(t1.eq(t3).show())\n"
         "    o.println(t1.ne(t3).show())\n"
         "    _\n",
         "derived_eq",
         "True\n"
         "False\n"
         "True\n");
}

/* The `==` operator lives on the value layer: the numeric slots read
   the runtime comparisons, String and Bytes read the derived eq. */
static void value_eq_operator(void)
{
    gate("main: (o: Out) ! =\n"
         "    o.println((1 == 1).show())\n"
         "    o.println((\"ab\" == \"ab\").show())\n"
         "    o.println((\"ab\" != \"ac\").show())\n"
         "    o.println((\"41\".to_bytes() == \"41\".to_bytes()).show())\n"
         "    _\n",
         "derived_veq",
         "True\n"
         "True\n"
         "True\n"
         "True\n");
}

static void string_order(void)
{
    gate("main: (o: Out) ! =\n"
         "    o.println((\"abc\" < \"abd\").show())\n"
         "    o.println((\"abd\" <= \"abd\").show())\n"
         "    o.println((\"b\" > \"a\").show())\n"
         "    _\n",
         "derived_ord",
         "True\n"
         "True\n"
         "True\n");
}

/* with takes its named fields and shares the rest: the original is
   unchanged. */
static void with_shares_unnamed(void)
{
    gate("Money: type = [cents: Int, cur: String]\n"
         "main: (o: Out) ! =\n"
         "    m: Money = {cents: 12500, cur: \"USD\"}\n"
         "    n = m.with({cents: 99})\n"
         "    o.println(n.show())\n"
         "    o.println(m.show())\n"
         "    _\n",
         "derived_with",
         "{cents: 99, cur: \"USD\"}\n"
         "{cents: 12500, cur: \"USD\"}\n");
}

/* The JSON mapping: a record and a string-keyed dictionary land as Obj
   with their names as keys, shown here in the constructor form the
   structural channel pins. */
static void json_rendering(void)
{
    gate("Point: type = [x: Int, y: String]\n"
         "main: (o: Out) ! =\n"
         "    p: Point = {x: 1, y: \"wd\"}\n"
         "    d: [String]Int = {\"a\": 1, \"b\": 2}\n"
         "    o.println(p.to_json().inspect())\n"
         "    o.println(d.to_json().inspect())\n"
         "    _\n",
         "derived_json",
         "Obj {\"x\": Int 1, \"y\": Str \"wd\"}\n"
         "Obj {\"a\": Int 1, \"b\": Int 2}\n");
}

/* The decode side inverts the mapping: the record round trips. */
static void json_round_trip(void)
{
    gate("Money: type = [cents: Int, cur: String]\n"
         "main: (o: Out) ! =\n"
         "    m: Money = {cents: 12500, cur: \"USD\"}\n"
         "    r = Money.from_json(m.to_json())\n"
         "    o.println(r.is_ok().show())\n"
         "    _\n",
         "derived_fromjson",
         "True\n");
}

/* A twelve-hundred-deep chain renders whole: the service terminates
   over a deep structure and prints the constructor form all the way
   down. */
static void deep_show_terminates(void)
{
    static char src[512];
    char *want = deep_expect(1200);

    snprintf(src, sizeof src,
             "Chain: type = End | Link Chain\n"
             "build: (n: Int) -> Chain =\n"
             "    case n:\n"
             "        0 -> End\n"
             "        _ -> Link build(n - 1)\n"
             "main: (o: Out) ! =\n"
             "    c = build(1200)\n"
             "    o.println(c.show())\n"
             "    _\n");
    if (want) {
        gate(src, "derived_deep", want);
    } else {
        EK_CHECK(0);
    }
    free(want);
}

int main(void)
{
    if (mkdir("build", 0777) != 0 && errno != EEXIST) {
        fprintf(stderr, "lower/run_derived: cannot create build/\n");
        return 1;
    }
    record_show();
    nested_show();
    adt_show();
    inspect_vs_show();
    tree_equality();
    value_eq_operator();
    string_order();
    with_shares_unnamed();
    json_rendering();
    json_round_trip();
    deep_show_terminates();
    return ek_test_report("lower/run_derived");
}
