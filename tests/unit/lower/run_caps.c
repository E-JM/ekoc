#define _POSIX_C_SOURCE 200809L

#include "common.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/wait.h>
#include <unistd.h>

/* The executable gates over the capability channel: every declared
   capability is supplied by position and the program's world contact
   lands on the redirected descriptors and files under build/. The
   suite assumes the repository root as its working directory. */

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
                 "cc -std=c99 -O2 -pthread -Irt build/%s.c build/libekort.a"
                 " -lm -o build/%s", name, name);
    if (n < 0 || (size_t)n >= sizeof cmd) {
        return 0;
    }
    return system(cmd) == 0;
}

/* Drains one descriptor into the buffer, NUL-terminated and capped. */
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

/* Lowers one program, and with a clean pipeline writes and compiles
   it; returns the binary's path, or NULL when the gate fails. */
static const char *build_gate(const char *src, const char *name)
{
    static char path[64];
    lfx f;
    int ok;

    lfx_begin(&f, src);
    ok = lfx_check_clean(&f) && lfx_lower_clean(&f) && f.low.text != NULL;
    if (ok) {
        snprintf(path, sizeof path, "build/%s.c", name);
        ok = write_unit(path, f.low.text, f.low.len);
    }
    lfx_end(&f);
    if (!ok) {
        EK_CHECK(0);
        return NULL;
    }
    EK_CHECK(compile_unit(name));
    snprintf(path, sizeof path, "build/%s", name);
    return path;
}

/* Runs the binary in a child whose standard input is the fixture and
   whose standard output and error are captured. argv are the extra
   arguments after the program name. */
static int run_gate(const char *path, const char *fixture,
                    const char **argv, char *out, size_t outcap,
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
        return -1;
    }
    if (pid == 0) {
        int in = fixture ? open(fixture, O_RDONLY) : STDOUT_FILENO;

        close(fds[0]);
        close(efs[0]);
        if (in < 0 || dup2(in, STDIN_FILENO) < 0
            || dup2(fds[1], STDOUT_FILENO) < 0
            || dup2(efs[1], STDERR_FILENO) < 0) {
            _exit(127);
        }
        if (fixture) {
            close(in);
        }
        close(fds[1]);
        close(efs[1]);
        execv(path, (char *const *)argv);
        _exit(127);
    }
    close(fds[1]);
    close(efs[1]);
    read_fd(fds[0], out, outcap);
    close(fds[0]);
    read_fd(efs[0], err, errcap);
    close(efs[0]);
    waitpid(pid, &status, 0);
    return status;
}

/* Runs a binary whose standard input is a pipe, writing one line into
   it after the delay and waiting up to timeout_ms. A program that
   outlives the timeout is killed and -1 is returned; otherwise the
   streams are captured and the wait status returned. */
static int run_gate_feeding(const char *path, const char *feed, int delay_ms,
                            int timeout_ms, char *out, size_t outcap,
                            char *err, size_t errcap)
{
    int inf[2], fds[2], efs[2];
    pid_t pid;
    int status = -1, waited = 0;

    out[0] = '\0';
    err[0] = '\0';
    if (pipe(inf) != 0 || pipe(fds) != 0 || pipe(efs) != 0) {
        return -1;
    }
    pid = fork();
    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        close(inf[1]);
        close(fds[0]);
        close(efs[0]);
        if (dup2(inf[0], STDIN_FILENO) < 0
            || dup2(fds[1], STDOUT_FILENO) < 0
            || dup2(efs[1], STDERR_FILENO) < 0) {
            _exit(127);
        }
        close(inf[0]);
        close(fds[1]);
        close(efs[1]);
        execl(path, path, (char *)NULL);
        _exit(127);
    }
    close(inf[0]);
    close(fds[1]);
    close(efs[1]);
    {
        struct timespec ts;

        ts.tv_sec = delay_ms / 1000;
        ts.tv_nsec = (long)(delay_ms % 1000) * 1000000L;
        nanosleep(&ts, NULL);
    }
    if (feed && feed[0]) {
        ssize_t n = write(inf[1], feed, strlen(feed));

        (void)n;
    }
    close(inf[1]);
    while (waited < timeout_ms) {
        if (waitpid(pid, &status, WNOHANG) == pid) {
            break;
        }
        {
            struct timespec ts = { 0, 10 * 1000 * 1000L };

            nanosleep(&ts, NULL);
        }
        waited += 10;
    }
    if (waited >= timeout_ms) {
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
        status = -1;
    }
    read_fd(fds[0], out, outcap);
    close(fds[0]);
    read_fd(efs[0], err, errcap);
    close(efs[0]);
    return status;
}

/* Builds and runs one gate over the fixture and argv; returns the wait
   status with the captured streams. */
static int gate(const char *src, const char *name, const char *fixture,
                const char **argv, char *out, size_t outcap, char *err,
                size_t errcap)
{
    const char *path = build_gate(src, name);

    if (!path) {
        return -1;
    }
    return run_gate(path, fixture, argv, out, outcap, err, errcap);
}

/* The session round trip: the write lands on stdout and the read comes
   back from standard input. */
static void out_in_round_trip(void)
{
    static const char *src =
        "main: (o: Out, i: In) ! =\n"
        "    o.println(\"ping\")\n"
        "    r = i.read_line()\n"
        "    case r:\n"
        "        Ok (Some line) -> o.println(\"got \" ++ line)\n"
        "        Ok None -> o.println(\"none\")\n"
        "        Err _ -> o.println(\"err\")\n"
        "    _\n";
    char out[64], err[8];
    int status;

    EK_CHECK(system("printf 'pong\\n' > build/caps_in.txt") == 0);
    status = gate(src, "caps_out_in", "build/caps_in.txt", NULL, out,
                  sizeof out, err, sizeof err);
    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, "ping\ngot pong\n");
    EK_CHECK_STR(err, "");
}

/* The line transport: a printed line reaches the stream once with one
   terminator, and a tee observer reports the bare line once per
   println, never twice and never with the terminator. */
static void line_transport(void)
{
    static const char *src =
        "main: (o: Out) ! =\n"
        "    t := o.tee((s): o.println(\"[\" ++ s ++ \"]\"))\n"
        "    t.println(\"one\")\n"
        "    t.print(\"two\")\n"
        "    t.println(\"\")\n"
        "    t.println(\"done\")\n"
        "    _\n";
    char out[128], err[8];
    int status;

    status = gate(src, "caps_line", NULL, NULL, out, sizeof out, err,
                  sizeof err);
    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, "one\n[one]\ntwo[two]\n\n[]\ndone\n[done]\n");
    EK_CHECK_STR(err, "");
}

/* The whole-path file family: write, read, stat, and exists. */
static void fs_whole_path(void)
{
    static const char *src =
        "main: (o: Out, fs: Fs) ! =\n"
        "    w = fs.write(\"build/caps_fs.txt\", \"written by fs\")\n"
        "    case w:\n"
        "        Ok _ -> o.println(\"wrote\")\n"
        "        Err e -> o.println(e.show())\n"
        "    s = fs.read(\"build/caps_fs.txt\")\n"
        "    case s:\n"
        "        Ok t -> o.println(t)\n"
        "        Err e -> o.println(e.show())\n"
        "    st = fs.stat(\"build/caps_fs.txt\")\n"
        "    case st:\n"
        "        Ok info -> o.println(\"size={info.size}\")\n"
        "        Err e -> o.println(e.show())\n"
        "    case fs.exists(\"build/caps_fs.txt\"):\n"
        "        True -> o.println(\"exists\")\n"
        "        False -> o.println(\"missing\")\n"
        "    _\n";
    char out[96], err[8];
    int status = gate(src, "caps_fs", NULL, NULL, out, sizeof out, err,
                      sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, "wrote\nwritten by fs\nsize=13\nexists\n");
    EK_CHECK_STR(err, "");
}

/* The environment and the argument vector, over a setenv and a fixed
   argv, in one manifest to see both supplied by position. */
static void env_and_args(void)
{
    static const char *src =
        "main: (o: Out, e: Env, a: Args) ! =\n"
        "    v = e.get(\"EKO_CAP_PROBE\")\n"
        "    case v:\n"
        "        Some x -> o.println(\"env=\" ++ x)\n"
        "        None -> o.println(\"noenv\")\n"
        "    case a.all():\n"
        "        {h, ..} -> o.println(\"arg=\" ++ h)\n"
        "        {} -> o.println(\"noargs\")\n"
        "    _\n";
    static const char *argv[] = {"bin", "zeta", NULL};
    char out[64], err[8];
    int status;

    EK_CHECK(setenv("EKO_CAP_PROBE", "yes42", 1) == 0);
    status = gate(src, "caps_env_args", NULL, argv, out, sizeof out, err,
                  sizeof err);
    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, "env=yes42\narg=zeta\n");
    EK_CHECK_STR(err, "");
}

/* The frozen clock: cap source over the injected pair, shown exactly.
   The companion import opens the time vocabulary. */
static void clock_frozen_shows_exactly(void)
{
    static const char *src =
        "import std/time\n"
        "main: (o: Out, c: Clock) ! =\n"
        "    f = Clock.frozen(Time 1792632540123000000)\n"
        "    o.println(f.now().show())\n"
        "    o.println(f.mono().show())\n"
        "    o.println(Duration.millis(1500).show())\n"
        "    _\n";
    char out[96], err[8];
    int status = gate(src, "caps_clock", NULL, NULL, out, sizeof out, err,
                      sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, "2026-10-22T01:29:00.123Z\nMono 0\n1.5s\n");
    EK_CHECK_STR(err, "");
}

/* The seeded derivation: one seed, one order, whatever the sequence
   is; another seed moves it, and the real bound of one holds. */
static void rand_seeds_fix_the_order(void)
{
    static const char *src =
        "main: (o: Out, r: Rand) ! =\n"
        "    g = Rand.with_seed(7)\n"
        "    a = g.int(1000)\n"
        "    b = g.int(1000)\n"
        "    h = Rand.with_seed(7)\n"
        "    k = Rand.with_seed(1)\n"
        "    case a == h.int(1000):\n"
        "        True -> o.println(\"det\")\n"
        "        False -> o.println(\"broken\")\n"
        "    case a == k.int(1000):\n"
        "        True -> o.println(\"same\")\n"
        "        False -> o.println(\"diff\")\n"
        "    o.println(\"draw={r.int(1)}\")\n"
        "    _ := b\n"
        "    _\n";
    char out[64], err[8];
    int status = gate(src, "caps_rand", NULL, NULL, out, sizeof out, err,
                      sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, "det\ndiff\ndraw=0\n");
    EK_CHECK_STR(err, "");
}

/* The process exit passes the program's code through. */
static void proc_exit_code(void)
{
    static const char *src =
        "main: (o: Out, p: Proc) ! =\n"
        "    o.println(\"bye\")\n"
        "    Proc.exit(p, 3)\n";
    char out[64], err[8];
    int status = gate(src, "caps_exit", NULL, NULL, out, sizeof out, err,
                      sizeof err);

    EK_CHECK_STR(out, "bye\n");
    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 3);
    }
}

/* The closed handle: the stale copy reports the reason, printed
   through the derived show. */
static void closed_handle_shows(void)
{
    static const char *src =
        "main: (o: Out, fs: Fs) ! =\n"
        "    h = fs.open(\"build/caps_closed.txt\", Write)\n"
        "    case h:\n"
        "        Ok f ->\n"
        "            _ := f.close()\n"
        "            r = f.write(\"x\")\n"
        "            case r:\n"
        "                Err e -> o.println(e.show())\n"
        "                Ok _ -> o.println(\"open?\")\n"
        "            _\n"
        "        Err e -> o.println(e.show())\n"
        "    _\n";
    char out[64], err[8];
    int status = gate(src, "caps_closed", NULL, NULL, out, sizeof out,
                      err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, "Closed\n");
    EK_CHECK_STR(err, "");
}

/* The advancing derivation: the lines come in order and end of input
   follows, forever after. */
static void in_from_lines(void)
{
    static const char *src =
        "main: (o: Out, i: In) ! =\n"
        "    g = In.from_lines({\"l1\", \"l2\"})\n"
        "    a = g.read_line()\n"
        "    b = g.read_line()\n"
        "    c = g.read_line()\n"
        "    case a:\n"
        "        Ok (Some t) -> o.println(\"1:\" ++ t)\n"
        "        _ -> o.println(\"1:?\")\n"
        "    case b:\n"
        "        Ok (Some t) -> o.println(\"2:\" ++ t)\n"
        "        _ -> o.println(\"2:?\")\n"
        "    case c:\n"
        "        Ok None -> o.println(\"3:none\")\n"
        "        _ -> o.println(\"3:?\")\n"
        "    _\n";
    char out[64], err[8];
    int status = gate(src, "caps_from_lines", NULL, NULL, out, sizeof out,
                      err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, "1:l1\n2:l2\n3:none\n");
    EK_CHECK_STR(err, "");
}

/* The scoping derivations: with_file closes on the value path, and the
   attenuated bundles refuse what the grant refuses. */
static void with_file_and_attenuation(void)
{
    static const char *src =
        "write_body: (fs: Fs) !-> Result _ IoError =\n"
        "    f <- fs.with_file(\"build/caps_scope.txt\", Write)\n"
        "        _ := f.write(\"scoped\")\n"
        "        _\n"
        "\n"
        "main: (o: Out, fs: Fs) ! =\n"
        "    w = write_body(fs)\n"
        "    case w:\n"
        "        Ok _ -> o.println(\"with ok\")\n"
        "        Err e -> o.println(e.show())\n"
        "    ro = fs.read_only()\n"
        "    refused = ro.write(\"build/caps_scope.txt\", \"n\")\n"
        "    case refused:\n"
        "        Ok _ -> o.println(\"wrote?!\")\n"
        "        Err e -> o.println(e.show())\n"
        "    rooted = fs.root(\"no_such_dir\")\n"
        "    case rooted.read(\"caps_scope.txt\"):\n"
        "        Ok _ -> o.println(\"leak\")\n"
        "        Err _ -> o.println(\"rooted\")\n"
        "    _\n";
    char out[96], err[8];
    int status = gate(src, "caps_scope", NULL, NULL, out, sizeof out, err,
                      sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, "with ok\nPermission\nrooted\n");
    EK_CHECK_STR(err, "");
}

/* The concurrency states: a pure task joins its result, a task
   cancelled while it stands at a world call joins None, and a task
   spinning in pure code is not interrupted, so the scope exit that
   waits for it never returns. */
static void conc_task_join_some(void)
{
    static const char *src =
        "work: () -> Int = 41 + 1\n"
        "\n"
        "main: (o: Out, c: Conc) ! =\n"
        "    r := c.scope((s):\n"
        "        t := s.spawn(work)\n"
        "        t.join())\n"
        "    o.println(r.show())\n";
    char out[32], err[8];
    int status = gate(src, "caps_conc_join", NULL, NULL, out, sizeof out,
                      err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, "Some 42\n");
    EK_CHECK_STR(err, "");
}

/* The child blocks at its first world call; the cancellation is
   pending before the feed arrives, so the call after the read is
   withheld and the task settles cancelled. */
static void conc_cancel_joins_none(void)
{
    static const char *src =
        "child: (i: In, o: Out) !-> _ =\n"
        "    case i.read_line():\n"
        "        Ok _ -> o.println(\"child read\")\n"
        "        Err _ -> o.println(\"child failed\")\n"
        "    _\n"
        "\n"
        "main: (o: Out, i: In, c: Conc) ! =\n"
        "    r := c.scope((s):\n"
        "        t := s.spawn((): child(i, o))\n"
        "        t.cancel()\n"
        "        o.println(\"cancelled\")\n"
        "        t.join())\n"
        "    o.println(r.show())\n";
    char out[64], err[8];
    int status = run_gate_feeding(build_gate(src, "caps_conc_cancel"),
                                  "line\n", 100, 3000, out, sizeof out, err,
                                  sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, "cancelled\nNone\n");
    EK_CHECK_STR(err, "");
}

/* The divergent child runs while the parent waits for a blocking child
   at a world call; when the callback returns, the scope exit cancels
   the spinner and waits, and pure code has no boundary to stop at. */
static void conc_divergence_hangs_scope_exit(void)
{
    static const char *src =
        "spin: (n: Int) -> Int = spin(n + 1)\n"
        "\n"
        "blocker: (i: In) !-> _ =\n"
        "    case i.read_line():\n"
        "        Ok _ -> _\n"
        "        Err _ -> _\n"
        "    _\n"
        "\n"
        "main: (o: Out, i: In, c: Conc) ! =\n"
        "    r := c.scope((s):\n"
        "        _ := s.spawn((): spin(0))\n"
        "        b := s.spawn((): blocker(i))\n"
        "        b.join()\n"
        "        7)\n"
        "    o.println(\"scope {r}\")\n";
    char out[32], err[8];
    const char *path = build_gate(src, "caps_conc_diverge");
    int status;

    if (!path) {
        return;
    }
    status = run_gate_feeding(path, "line\n", 300, 1200, out, sizeof out,
                              err, sizeof err);
    EK_CHECK_U64((unsigned)status, (unsigned)-1);
    EK_CHECK_STR(out, "");
}

int main(void)
{
    /* A child that settles before the feed closes its end: the write is
       then a lost line, not a death of the test process. */
    signal(SIGPIPE, SIG_IGN);
    if (mkdir("build", 0777) != 0 && errno != EEXIST) {
        fprintf(stderr, "lower/run_caps: cannot create build/\n");
        return 1;
    }
    out_in_round_trip();
    line_transport();
    fs_whole_path();
    env_and_args();
    clock_frozen_shows_exactly();
    rand_seeds_fix_the_order();
    proc_exit_code();
    closed_handle_shows();
    in_from_lines();
    with_file_and_attenuation();
    conc_task_join_some();
    conc_cancel_joins_none();
    conc_divergence_hangs_scope_exit();
    return ek_test_report("lower/run_caps");
}
