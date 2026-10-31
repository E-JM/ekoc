#define _POSIX_C_SOURCE 200809L

#include "harness.h"
#include "capident.h"

#include <stddef.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ekort.h"

/* The reader's terminating shape, exercised in a forked child: a
   document nesting past the cap panics with P-panic instead of
   exhausting the C stack, and the deepest clean document parses. The
   parent checks the captured text and the wait status. */

#define TEXT_CAP 4096

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

/* The text of depth nested containers, either both brackets or the
   opener only. */
static void nest_text(char *out, size_t depth, int closed)
{
    size_t i;

    for (i = 0; i < depth; i++)
        out[i] = '[';
    if (closed) {
        for (i = 0; i < depth; i++)
            out[depth + i] = ']';
        out[2 * depth] = '\0';
    } else {
        out[depth] = '\0';
    }
}

/* The deepest clean document parses and releases: the child reports
   itself by exiting zero. The cap the reader's contract states is 512
   container levels. */
static void body_cap_minus_one_parses(void)
{
    char text[1200];
    ek_obj *in;
    ek_val r;

    nest_text(text, 512, 1);
    in = ek_str_from_cstr(text);
    r = ek_json_parse(in);
    ek_release(r);
    ek_release(ek_val_ref(in));
    _exit(0);
}

static void body_past_the_cap_panics(void)
{
    char text[TEXT_CAP];
    ek_obj *in;
    ek_val r;

    nest_text(text, 600, 0);
    in = ek_str_from_cstr(text);
    r = ek_json_parse(in);
    (void)r;
    _exit(0); /* a reader that returns failed to terminate */
}

int main(void)
{
    child_run run;

    run_child(body_cap_minus_one_parses, &run);
    EK_CHECK(WIFEXITED(run.status));
    EK_CHECK_U64(WEXITSTATUS(run.status), 0);
    EK_CHECK_STR(run.text, "");

    run_child(body_past_the_cap_panics, &run);
    EK_CHECK(WIFEXITED(run.status));
    EK_CHECK_U64(WEXITSTATUS(run.status), 1);
    EK_CHECK_STR(run.text, "panic [P-panic]: json nesting too deep\n");
    return ek_test_report("rt/json_panic");
}
