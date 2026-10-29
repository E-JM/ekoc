#define _POSIX_C_SOURCE 200809L

#include "harness.h"

#include <signal.h>
#include <stddef.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ekort.h"

/* Panic and abort terminate the process without unwinding. A forked
   child calls the entry point with standard error captured through a
   pipe; the parent checks the text and the wait status. */

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

static void body_panic_message(void)
{
    ek_panic("P-panic", "boom");
}

static void body_panic_division(void)
{
    ek_panic("P-div", "division by zero");
}

static void body_abort(void)
{
    ek_abort("stack overflow");
}

static void panic_writes_first_line_and_exits_one(void)
{
    child_run run;

    run_child(body_panic_message, &run);
    EK_CHECK(WIFEXITED(run.status));
    EK_CHECK_U64(WEXITSTATUS(run.status), 1);
    EK_CHECK_STR(run.text, "panic [P-panic]: boom\n");
}

/* Any code travels the same shape, and the message is verbatim. */
static void panic_carries_the_code_verbatim(void)
{
    child_run run;

    run_child(body_panic_division, &run);
    EK_CHECK(WIFEXITED(run.status));
    EK_CHECK_U64(WEXITSTATUS(run.status), 1);
    EK_CHECK_STR(run.text, "panic [P-div]: division by zero\n");
}

static void abort_signals_and_reports_reason(void)
{
    child_run run;

    run_child(body_abort, &run);
    EK_CHECK(WIFSIGNALED(run.status));
    EK_CHECK_U64(WTERMSIG(run.status), SIGABRT);
    EK_CHECK_STR(run.text, "abort: stack overflow\n");
}

int main(void)
{
    panic_writes_first_line_and_exits_one();
    panic_carries_the_code_verbatim();
    abort_signals_and_reports_reason();
    return ek_test_report("rt/panic");
}
