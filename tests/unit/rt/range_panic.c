#define _POSIX_C_SOURCE 200809L

#include "harness.h"

#include <stdint.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ekort.h"

/* The range traversals' terminating shapes, exercised in forked
   children: an empty span invokes the callback never, a single closed
   element once, and the count through the checked arithmetic panics
   on the same overflows the language's own arithmetic does. The
   parent checks the captured text and the wait status. */

#define TEXT_CAP 512

typedef struct {
    int status;
    char text[TEXT_CAP];
} child_run;

static void read_all(int fd, char *buf, size_t cap)
{
    size_t len = 0;
    ssize_t n;

    while (len + 1 < cap) {
        n = read(fd, buf + len, cap - 1 - len);
        if (n <= 0)
            break;
        len += (size_t)n;
    }
    buf[len] = '\0';
}

/* Runs body in a child whose standard error is the pipe, then records
   the wait status and everything the child wrote. */
static void run_child(void (*body)(void), child_run *run)
{
    int fds[2];
    pid_t pid;

    run->status = 0;
    run->text[0] = '\0';
    if (pipe(fds) != 0) {
        EK_CHECK(0);
        return;
    }
    pid = fork();
    if (pid < 0) {
        EK_CHECK(0);
        close(fds[0]);
        close(fds[1]);
        return;
    }
    if (pid == 0) {
        close(fds[0]);
        if (dup2(fds[1], STDERR_FILENO) < 0)
            _exit(127);
        close(fds[1]);
        body();
        _exit(127); /* a body that returns failed to terminate */
    }
    close(fds[1]);
    read_all(fds[0], run->text, TEXT_CAP);
    close(fds[0]);
    waitpid(pid, &run->status, 0);
}

/* The terminating shape: exit status 1 and the exact first line. */
static void check_panic(void (*body)(void), const char *line)
{
    child_run run;

    run_child(body, &run);
    EK_CHECK(WIFEXITED(run.status));
    EK_CHECK_U64(WEXITSTATUS(run.status), 1);
    EK_CHECK_STR(run.text, line);
}

/* The mirror of the generated range struct, per the layout contract
   in rt/ekort.h. */
typedef struct {
    ek_obj obj;
    ek_val start;
    ek_val end;
    ek_val closed;
} test_range;

static void test_range_drop(ek_obj *obj)
{
    test_range *r = (test_range *)obj;

    ek_release(r->start);
    ek_release(r->end);
    ek_release(r->closed);
}

static const ek_typeinfo test_range_info = { EK_KIND_STRUCT,
                                             test_range_drop };

static ek_val make_range(ek_val start, ek_val end, int closed)
{
    test_range *r = calloc(1, sizeof *r);

    r->obj.info = &test_range_info;
    r->obj.rc = 1;
    r->start = start;
    r->end = end;
    r->closed = ek_bool(closed);
    return ek_val_ref(&r->obj);
}

static ek_val int_range(int64_t a, int64_t b, int closed)
{
    return make_range(ek_val_int(a), ek_val_int(b), closed);
}

static ek_val uint_range(uint64_t a, uint64_t b, int closed)
{
    return make_range(ek_val_uint(a), ek_val_uint(b), closed);
}

static ek_val die_fn(ek_val env, ek_val *args, size_t argc)
{
    (void)env;
    (void)args;
    (void)argc;
    ek_panic("P-probe", "the callback ran");
    return ek_val_unit();
}

static void body_empty_each_runs_nothing(void)
{
    ek_val f = ek_closure_new(die_fn, ek_val_unit());
    ek_val r = int_range(5, 5, 0);
    ek_val u = ek_range_each(r, f);

    EK_CHECK(ek_val_is_unit(u));
    ek_release(u);
    ek_release(r);
    ek_release(f);
    _exit(0); /* a clean child reports itself */
}

static void body_single_each_runs_once(void)
{
    ek_val f = ek_closure_new(die_fn, ek_val_unit());
    ek_val r = int_range(5, 5, 1);

    ek_release(ek_range_each(r, f));
    ek_release(r);
    ek_release(f);
}

static void body_span_overflow(void)
{
    ek_val n = ek_range_length(int_range(INT64_MIN, INT64_MAX, 0));

    (void)n;
}

static void body_uint_span_overflow(void)
{
    ek_val n = ek_range_length(uint_range(0, UINT64_MAX, 1));

    (void)n;
}

/* The clean shape: an empty span visits nothing, so the panicking
   callback never runs and the child exits zero. */
static void empty_each_stays_clean(void)
{
    child_run run;

    run_child(body_empty_each_runs_nothing, &run);
    EK_CHECK(WIFEXITED(run.status));
    EK_CHECK_U64(WEXITSTATUS(run.status), 0);
    EK_CHECK_STR(run.text, "");
}

static void single_each_panics_once(void)
{
    check_panic(body_single_each_runs_once,
                "panic [P-probe]: the callback ran\n");
}

static void span_overflow_panics(void)
{
    check_panic(body_span_overflow,
                "panic [P-overflow]: arithmetic overflow\n");
}

static void uint_span_overflow_panics(void)
{
    check_panic(body_uint_span_overflow,
                "panic [P-overflow]: arithmetic overflow\n");
}

int main(void)
{
    empty_each_stays_clean();
    single_each_panics_once();
    span_overflow_panics();
    uint_span_overflow_panics();
    return ek_test_report("rt/range_panic");
}
