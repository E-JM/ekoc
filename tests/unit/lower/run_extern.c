#define _POSIX_C_SOURCE 200809L

#include "common.h"
#include "ext_cases.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* The executable gates of the native interface: the emitted unit and
   its shim compile together against the runtime archive, the shim
   answering the extern symbols the unit's prototypes declare. The
   suite assumes the repository root as its working directory. */

/* A clean exit pins one probe condition inside the program. */
static void expect_clean(const char *what, int status)
{
    (void)what;
    EK_CHECK(status >= 0 && WIFEXITED(status));
    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
}

static int write_file(const char *path, const char *text, size_t len)
{
    FILE *fp = fopen(path, "wb");
    int ok;

    if (!fp) {
        return 0;
    }
    ok = fwrite(text, 1, len, fp) == len;
    return fclose(fp) == 0 && ok;
}

/* Compiles the unit with its shim and the runtime archive. */
static int compile_gate(const char *name)
{
    char cmd[300];
    int n;

    n = snprintf(cmd, sizeof cmd,
                 "cc -std=c99 -O2 -pthread -Irt build/%s.c build/%s_shim.c"
                 " build/libekort.a -lm -pthread -o build/%s", name, name, name);
    if (n < 0 || (size_t)n >= sizeof cmd) {
        return 0;
    }
    return system(cmd) == 0;
}

static void read_fd(int fd, char *buf, size_t cap)
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

/* Runs the gate binary in a child with the standard error captured;
   returns the wait status. */
static int run_gate(const char *name, char *err, size_t cap)
{
    char path[64];
    int fds[2];
    pid_t pid;
    int status = -1;

    err[0] = '\0';
    if (pipe(fds) != 0) {
        return -1;
    }
    pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return -1;
    }
    if (pid == 0) {
        close(fds[0]);
        if (dup2(fds[1], STDERR_FILENO) < 0) {
            _exit(127);
        }
        close(fds[1]);
        snprintf(path, sizeof path, "build/%s", name);
        execl(path, path, (char *)NULL);
        _exit(127);
    }
    close(fds[1]);
    read_fd(fds[0], err, cap);
    close(fds[0]);
    waitpid(pid, &status, 0);
    return status;
}

/* Lowers one program, and with a clean pipeline writes the unit and
   the shim, and compiles them together; runs the binary and returns
   the status with the captured standard error. */
static int gate(const char *src, const char *shim, const char *name,
                char *err, size_t cap)
{
    lfx f;
    char path[64];
    int ok;

    lfx_begin(&f, src);
    ok = lfx_check_clean(&f) && lfx_lower_clean(&f) && f.low.text != NULL;
    if (ok) {
        snprintf(path, sizeof path, "build/%s.c", name);
        ok = write_file(path, f.low.text, f.low.len);
    }
    lfx_end(&f);
    if (!ok) {
        EK_CHECK(0);
        return -1;
    }
    snprintf(path, sizeof path, "build/%s_shim.c", name);
    if (!write_file(path, "#include <stddef.h>\n#include <stdint.h>\n\n",
                    strlen("#include <stddef.h>\n#include <stdint.h>\n\n"))) {
        EK_CHECK(0);
        return -1;
    }
    {
        FILE *fp = fopen(path, "ab");

        if (!fp || fwrite(shim, 1, strlen(shim), fp) != strlen(shim)
            || fclose(fp) != 0) {
            EK_CHECK(0);
            return -1;
        }
    }
    EK_CHECK(compile_gate(name));
    return run_gate(name, err, cap);
}

/* The libm sqrt through a Pure extern: the domain word rounds trips
   through the F64 width. */
static void sqrt_from_libm(void)
{
    static const char *src =
        "_sqrt: (x: F64) -> F64 = extern \"sqrt\" Pure\n"
        "main: () -> _ =\n"
        "    r = _sqrt(2.25.to_f64())\n"
        "    rf = Float.from_f64(r)\n"
        "    w = 0\n"
        "    if rf == 1.5:\n"
        "        w := 1\n"
        "        _\n"
        "    d = 1.0 / w.to_float()\n"
        "    _ := d\n"
        "    _\n";
    char err[8];

    expect_clean("sqrt", gate(src, "", "ext_sqrt", err, sizeof err));
    EK_CHECK_STR(err, "");
}

/* The String parameter reads the bytes and the result copies fresh:
   the uppercase lands, and a result outside UTF-8 panics with the
   boundary's own line. */
static void string_boundary(void)
{
    char err[256];
    int status = gate(prog_shout, shim_shout, "ext_shout", err,
                      sizeof err);
    const char *line_end;

    expect_clean("shout", status);
    EK_CHECK_STR(err, "");
    status = gate(prog_bad_string, shim_shout, "ext_badstr", err,
                  sizeof err);
    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 1);
    }
    line_end = strchr(err, '\n');
    EK_CHECK(line_end != NULL);
    if (line_end) {
        size_t n = (size_t)(line_end - err);

        EK_CHECK(n == strlen("panic [P-panic]: a String result is not UTF-8")
                 && strncmp(err, "panic [P-panic]: a String result is not "
                                "UTF-8", n) == 0);
    }
}

/* The qsort shape: the comparator closure captures one binding, the
   adapter carries it through the context, and the shim sorts through
   the pair. A broken context reads garbage and the probe fails. */
static void callback_sorts(void)
{
    char err[8];

    expect_clean("sort", gate(prog_sort, shim_sort, "ext_sort", err,
                              sizeof err));
    EK_CHECK_STR(err, "");
}

/* The integer word ferries in and out through its exact width. */
static void word_echo(void)
{
    static const char *src =
        "_one: () -> I32 = extern \"shim_one\" Pure\n"
        "_echo: (x: I32) -> I32 = extern \"shim_echo\" Pure\n"
        "main: () -> _ =\n"
        "    r = _echo(_one())\n"
        "    ri = Int.from_i32(r)\n"
        "    w = 0\n"
        "    if ri == 2:\n"
        "        w := 1\n"
        "        _\n"
        "    d = 1 / w\n"
        "    _ := d\n"
        "    _\n";
    static const char *shim =
        "int32_t shim_one(void)\n"
        "{\n"
        "    return 1;\n"
        "}\n"
        "int32_t shim_echo(int32_t x)\n"
        "{\n"
        "    return (int32_t)(x + 1);\n"
        "}\n";
    char err[8];

    expect_clean("word", gate(src, shim, "ext_word", err, sizeof err));
    EK_CHECK_STR(err, "");
}

/* The ctype round trip: the shim mutates its own copy, the original
   stays, and the returned one changed. */
static void ctype_round_trip(void)
{
    static const char *src =
        "Stat: ctype = [ok: I32, size: I64, mode: U32]\n"
        "_bump: (p: Stat) !-> Stat = extern \"shim_bump\"\n"
        "main: () ! =\n"
        "    p: Stat = {ok: 7, size: 8, mode: 9}\n"
        "    q = _bump(p)\n"
        "    a = Int.from_i32(p.ok)\n"
        "    b = Int.from_i64(p.size)\n"
        "    c = Int.from_i32(q.ok)\n"
        "    e = Int.from_i64(q.size)\n"
        "    w = 0\n"
        "    if a == 7:\n"
        "        if b == 8:\n"
        "            if c == 8:\n"
        "                if e == 9:\n"
        "                    w := 1\n"
        "                    _\n"
        "    d = 1 / w\n"
        "    _ := d\n"
        "    _\n";
    static const char *shim =
        "typedef struct { int32_t f_ok; int64_t f_size; uint32_t f_mode; "
        "} ekC_test$Stat;\n"
        "ekC_test$Stat shim_bump(ekC_test$Stat p)\n"
        "{\n"
        "    p.f_ok += 1;\n"
        "    p.f_size += 1;\n"
        "    p.f_mode += 1;\n"
        "    return p;\n"
        "}\n";
    char err[8];

    expect_clean("ctype", gate(src, shim, "ext_ctype", err, sizeof err));
    EK_CHECK_STR(err, "");
}

/* The copy-in borrow: the shim reads the pointed-to struct only. */
static void c_in_read_borrow(void)
{
    static const char *src =
        "Pair: ctype = [a: I32, b: I32]\n"
        "_peek: (p: C.In Pair) -> I32 = extern \"shim_peek\" Pure\n"
        "main: () -> _ =\n"
        "    p: Pair = {a: 7, b: 8}\n"
        "    r = _peek(p)\n"
        "    ri = Int.from_i32(r)\n"
        "    w = 0\n"
        "    if ri == 78:\n"
        "        w := 1\n"
        "        _\n"
        "    d = 1 / w\n"
        "    _ := d\n"
        "    _\n";
    static const char *shim =
        "typedef struct { int32_t f_a; int32_t f_b; } ekC_test$Pair;\n"
        "int32_t shim_peek(const ekC_test$Pair *p)\n"
        "{\n"
        "    return (int32_t)(p->f_a * 10 + p->f_b);\n"
        "}\n";
    char err[8];

    expect_clean("c_in", gate(src, shim, "ext_cin", err, sizeof err));
    EK_CHECK_STR(err, "");
}

/* The element-array row: the caller's list copies into a C element
   array valid for the call, and the callee reads it element by
   element. */
static void c_in_element_array(void)
{
    static const char *src =
        "_sum: (xs: C.In []I32, n: I64) -> I64 = extern \"shim_sum\" "
        "Pure\n"
        "main: () -> _ =\n"
        "    xs: []I32 = {10, 20, 30}\n"
        "    r = Int.from_i64(_sum(xs, 3))\n"
        "    w = 0\n"
        "    if r == 60:\n"
        "        w := 1\n"
        "        _\n"
        "    d = 1 / w\n"
        "    _ := d\n"
        "    _\n";
    static const char *shim =
        "int64_t shim_sum(const int32_t *xs, int64_t n)\n"
        "{\n"
        "    int64_t acc = 0;\n"
        "    int64_t i;\n"
        "    for (i = 0; i < n; i++) {\n"
        "        acc += xs[i];\n"
        "    }\n"
        "    return acc;\n"
        "}\n";
    char err[8];

    expect_clean("cin list", gate(src, shim, "ext_cin_list", err,
                                   sizeof err));
    EK_CHECK_STR(err, "");
}

/* The opaque pointer crosses, returns, and still unwraps to the same
   storage through both values. */
static void ptr_round_trip(void)
{
    static const char *src =
        "_box: (n: I64) -> C.Ptr.Void = extern \"shim_box\" Pure\n"
        "_pid: (p: C.Ptr.Void) -> C.Ptr.Void = extern \"shim_pid\" Pure\n"
        "_unbox: (p: C.Ptr.Void) -> I32 = extern \"shim_unbox\" Pure\n"
        "main: () -> _ =\n"
        "    p = _box(42)\n"
        "    q = _pid(p)\n"
        "    x = Int.from_i32(_unbox(p))\n"
        "    y = Int.from_i32(_unbox(q))\n"
        "    w = 0\n"
        "    if x == 42:\n"
        "        if y == 42:\n"
        "            w := 1\n"
        "            _\n"
        "    d = 1 / w\n"
        "    _ := d\n"
        "    _\n";
    static const char *shim =
        "static int slot;\n"
        "void *shim_box(int64_t n)\n"
        "{\n"
        "    slot = (int)n;\n"
        "    return &slot;\n"
        "}\n"
        "void *shim_pid(void *p)\n"
        "{\n"
        "    return p;\n"
        "}\n"
        "int32_t shim_unbox(void *p)\n"
        "{\n"
        "    return (int32_t)*(int *)p;\n"
        "}\n";
    char err[8];

    expect_clean("ptr", gate(src, shim, "ext_ptr", err, sizeof err));
    EK_CHECK_STR(err, "");
}

/* A ctype carrying the opaque pointer: the field packs and unpacks
   across the boundary, the callee reads through it, and the pointer
   itself never renders an address. */
static void ptr_ctype_field(void)
{
    static const char *src =
        "H: ctype = [raw: C.Ptr.Void, n: I32]\n"
        "_get: () -> H = extern \"shim_get_h\" Pure\n"
        "_read: (h: C.In H) -> I32 = extern \"shim_read_h\" Pure\n"
        "main: () -> _ =\n"
        "    h = _get()\n"
        "    r = Int.from_i32(_read(h))\n"
        "    s = h.show()\n"
        "    w = 0\n"
        "    if r == 7:\n"
        "        if s == \"\\{raw: <opaque>, n: <opaque>}\":\n"
        "            w := 1\n"
        "            _\n"
        "    d = 1 / w\n"
        "    _ := d\n"
        "    _\n";
    static const char *shim =
        "typedef struct { void *f_raw; int32_t f_n; } ekC_test$H;\n"
        "static ekC_test$H the_h = { (void *)1, 7 };\n"
        "ekC_test$H shim_get_h(void)\n"
        "{\n"
        "    return the_h;\n"
        "}\n"
        "int32_t shim_read_h(const ekC_test$H *h)\n"
        "{\n"
        "    return h->f_n;\n"
        "}\n";
    char err[8];

    expect_clean("ptr field", gate(src, shim, "ext_ptr_field", err,
                                   sizeof err));
    EK_CHECK_STR(err, "");
}

/* The unit result lowers as the statement call: the write lands and
   reads back through a second extern. */
static void unit_result_statement(void)
{
    static const char *src =
        "_mark: (n: I64) ! = extern \"shim_mark\"\n"
        "_read: () -> I64 = extern \"shim_read\" Pure\n"
        "main: () ! =\n"
        "    _mark(7)\n"
        "    x = Int.from_i64(_read())\n"
        "    w = 0\n"
        "    if x == 7:\n"
        "        w := 1\n"
        "        _\n"
        "    d = 1 / w\n"
        "    _ := d\n"
        "    _\n";
    static const char *shim =
        "static int64_t marked;\n"
        "void shim_mark(int64_t n)\n"
        "{\n"
        "    marked = n;\n"
        "}\n"
        "int64_t shim_read(void)\n"
        "{\n"
        "    return marked;\n"
        "}\n";
    char err[8];

    expect_clean("unit", gate(src, shim, "ext_unit", err, sizeof err));
    EK_CHECK_STR(err, "");
}

int main(void)
{
    if (mkdir("build", 0777) != 0 && errno != EEXIST) {
        fprintf(stderr, "lower/run_extern: cannot create build/\n");
        return 1;
    }
    sqrt_from_libm();
    string_boundary();
    word_echo();
    ctype_round_trip();
    c_in_read_borrow();
    c_in_element_array();
    ptr_ctype_field();
    callback_sorts();
    ptr_round_trip();
    unit_result_statement();
    return ek_test_report("lower/run_extern");
}
