#define _POSIX_C_SOURCE 200809L

#include "harness.h"

#include <signal.h>
#include <stddef.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ekort.h"

/* The closure family: the environment is retained at capture and
   released at reclamation, calls pass the environment and the
   arguments through the uniform ABI, and calling a non-closure
   panics. A forked child exercises the panic; the parent checks the
   text and the wait status. */

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

/* Returns the captured environment unchanged. */
static ek_val identity_fn(ek_val env, ek_val *args, size_t argc)
{
    (void)args;
    (void)argc;
    return env;
}

/* Adds the environment to the two arguments. */
static ek_val add_fn(ek_val env, ek_val *args, size_t argc)
{
    if (argc != 2)
        return env;
    return ek_val_int(env.as.i + args[0].as.i + args[1].as.i);
}

static void environment_is_retained(void)
{
    ek_obj *env = ek_str_from_cstr("captured");
    ek_val c = ek_closure_new(identity_fn, ek_val_ref(env));

    EK_CHECK_U64(c.as.ref->rc, 1);
    EK_CHECK_U64(env->rc, 2); /* the caller's and the closure's */
    ek_release(c);
    EK_CHECK_U64(env->rc, 1);
    ek_release(ek_val_ref(env));
}

static void call_passes_environment_and_arguments(void)
{
    ek_val c = ek_closure_new(add_fn, ek_val_int(10));
    ek_val args[2] = { ek_val_int(1), ek_val_int(2) };
    ek_val r = ek_closure_call(c, args, 2);

    EK_CHECK(ek_val_is_int(r));
    EK_CHECK_U64(r.as.i, 13);
    r = ek_closure_call(c, NULL, 0);
    EK_CHECK_U64(r.as.i, 10); /* add_fn answers with the environment */
    ek_release(c);
}

static void call_borrows_the_environment(void)
{
    ek_obj *env = ek_str_from_cstr("env");
    ek_val c = ek_closure_new(identity_fn, ek_val_ref(env));
    ek_val r = ek_closure_call(c, NULL, 0);

    EK_CHECK(r.as.ref == env); /* returned as given, no new reference */
    EK_CHECK_U64(env->rc, 2);
    ek_release(c);
    ek_release(ek_val_ref(env));
}

static void environment_is_a_value_like_any_other(void)
{
    ek_val env = ek_cons(ek_val_int(1), ek_nil_val());
    ek_val c = ek_closure_new(identity_fn, env);

    EK_CHECK_U64(env.as.ref->rc, 2); /* the caller's and the closure's */
    ek_release(c);
    EK_CHECK_U64(env.as.ref->rc, 1);
    ek_release(env);
}

static void body_call_an_int(void)
{
    ek_closure_call(ek_val_int(5), NULL, 0);
}

static void body_call_nil(void)
{
    ek_closure_call(ek_nil_val(), NULL, 0);
}

static void body_call_a_string(void)
{
    ek_obj *s = ek_str_from_cstr("s");

    ek_closure_call(ek_val_ref(s), NULL, 0);
}

static void non_closure_panics(void)
{
    child_run run;
    int i;
    void (*bodies[3])(void) = { body_call_an_int, body_call_nil,
                                body_call_a_string };

    for (i = 0; i < 3; i++) {
        run_child(bodies[i], &run);
        EK_CHECK(WIFEXITED(run.status));
        EK_CHECK_U64(WEXITSTATUS(run.status), 1);
        EK_CHECK_STR(run.text, "panic [P-panic]: call of a non-closure\n");
    }
}

int main(void)
{
    environment_is_retained();
    call_passes_environment_and_arguments();
    call_borrows_the_environment();
    environment_is_a_value_like_any_other();
    non_closure_panics();
    return ek_test_report("rt/closure");
}
