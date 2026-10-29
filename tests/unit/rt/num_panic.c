#define _POSIX_C_SOURCE 200809L

#include "harness.h"

#include <stddef.h>
#include <stdint.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ekort.h"

/* The checked word entries and ek_panic_value terminate the process.
   A forked child triggers the case with standard error captured
   through a pipe; the parent checks the text and the wait status. */

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

static void body_int_div_zero(void)
{
    ek_num_div(ek_val_int(1), ek_val_int(0));
}

static void body_int_mod_zero(void)
{
    ek_num_mod(ek_val_int(1), ek_val_int(0));
}

static void body_uint_div_zero(void)
{
    ek_num_div(ek_val_uint(1), ek_val_uint(0));
}

static void body_uint_mod_zero(void)
{
    ek_num_mod(ek_val_uint(1), ek_val_uint(0));
}

static void body_int_add_overflow(void)
{
    ek_num_add(ek_val_int(INT64_MAX), ek_val_int(1));
}

static void body_int_sub_overflow(void)
{
    ek_num_sub(ek_val_int(INT64_MIN), ek_val_int(1));
}

static void body_int_div_min_minus_one(void)
{
    ek_num_div(ek_val_int(INT64_MIN), ek_val_int(-1));
}

static void body_int_mod_min_minus_one(void)
{
    ek_num_mod(ek_val_int(INT64_MIN), ek_val_int(-1));
}

static void body_int_neg_min(void)
{
    ek_num_neg(ek_val_int(INT64_MIN));
}

static void body_uint_mul_overflow(void)
{
    ek_num_mul(ek_val_uint(UINT64_MAX), ek_val_uint(2));
}

static void body_uint_sub_overflow(void)
{
    ek_num_sub(ek_val_uint(0), ek_val_uint(1));
}

static void body_panic_string(void)
{
    ek_panic_value(ek_val_ref(ek_str_from_cstr("boom")));
}

static void body_panic_not_string(void)
{
    ek_panic_value(ek_val_int(5));
}

int main(void)
{
    check_panic(body_int_div_zero, "panic [P-div]: division by zero\n");
    check_panic(body_int_mod_zero, "panic [P-div]: division by zero\n");
    check_panic(body_uint_div_zero, "panic [P-div]: division by zero\n");
    check_panic(body_uint_mod_zero, "panic [P-div]: division by zero\n");
    check_panic(body_int_add_overflow,
                "panic [P-overflow]: arithmetic overflow\n");
    check_panic(body_int_sub_overflow,
                "panic [P-overflow]: arithmetic overflow\n");
    check_panic(body_int_div_min_minus_one,
                "panic [P-overflow]: arithmetic overflow\n");
    check_panic(body_int_mod_min_minus_one,
                "panic [P-overflow]: arithmetic overflow\n");
    check_panic(body_int_neg_min,
                "panic [P-overflow]: arithmetic overflow\n");
    check_panic(body_uint_mul_overflow,
                "panic [P-overflow]: arithmetic overflow\n");
    check_panic(body_uint_sub_overflow,
                "panic [P-overflow]: arithmetic overflow\n");
    check_panic(body_panic_string, "panic [P-panic]: boom\n");
    check_panic(body_panic_not_string,
                "panic [P-panic]: panic of a non-string\n");
    return ek_test_report("rt/num_panic");
}
