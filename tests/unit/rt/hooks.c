#include "harness.h"

#include <signal.h>
#include <stddef.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ekort.h"

#include "keyfam.h"

/* The key hooks a container carries: a family's eq and hash pair bind
   its values into one key domain, the text and bytes families key by
   their bytes under the default pair, and a reference key the carried
   pair cannot answer aborts the process. The container side -- the
   keying of dictionaries and sets over carried hooks, the Float
   rules, the pair's clone across inserts -- is keys.c's subject. */

static void hooks_answer_over_the_layout(void)
{
    ek_val a = pair_of(&pair_a_info, 1, 2);
    ek_val b = pair_of(&pair_a_info, 1, 2);
    ek_val c = pair_of(&pair_a_info, 1, 3);
    unsigned long ha = a.as.ref->info->hash(a);
    unsigned long hb = b.as.ref->info->hash(b);

    EK_CHECK(a.as.ref != b.as.ref);
    EK_CHECK(pair_eq(a, b));
    EK_CHECK(!pair_eq(a, c));
    EK_CHECK_U64(ha, hb); /* equal values hash equal */
    EK_CHECK(pair_hash(a) == ha); /* the identity holds over the run */
    EK_CHECK_U64(pair_hash(a) == pair_hash(c), 0);
    EK_CHECK(pair_eq(a, a));
    ek_release(a);
    ek_release(b);
    ek_release(c);
}

/* The text hooks: two spellings of one text are one key. */
static void text_keys_stay_byte_based(void)
{
    ek_obj *k1 = ek_str_from_cstr("key");
    ek_obj *k2 = ek_str_from_cstr("key");
    ek_val d0 = ek_dict_new(ek_key_eq, ek_key_hash);
    ek_val d1 = ek_dict_set(d0, ek_val_ref(k1), ek_val_int(1));
    ek_val d2 = ek_dict_set(d1, ek_val_ref(k2), ek_val_int(2));
    ek_val out = ek_val_unit();

    EK_CHECK_U64(ek_dict_len(d2), 1);
    EK_CHECK(ek_dict_get(d2, ek_val_ref(k2), &out));
    EK_CHECK_U64(out.as.i, 2);
    ek_release(d2);
    ek_release(d1);
    ek_release(d0);
    ek_release(ek_val_ref(k1));
    ek_release(ek_val_ref(k2));
}

/* The text hooks on a set: two spellings of one text are one element,
   and a String and a Bytes value over one byte sequence stay apart. */
static void set_text_hooks(void)
{
    ek_obj *t1 = ek_str_from_cstr("k");
    ek_obj *t2 = ek_str_from_cstr("k");
    ek_obj *b = ek_bytes_new("k", 1);
    ek_val s0 = ek_set_new(ek_key_eq, ek_key_hash);
    ek_val s1 = ek_set_insert(s0, ek_val_ref(t1));
    ek_val s2 = ek_set_insert(s1, ek_val_ref(t2));
    ek_val s3 = ek_set_insert(s2, ek_val_ref(b));

    EK_CHECK_U64(ek_set_len(s2), 1);
    EK_CHECK_U64(ek_set_len(s3), 2); /* Bytes stays a family apart */
    ek_release(s3);
    ek_release(s2);
    ek_release(s1);
    ek_release(s0);
    ek_release(ek_val_ref(t1));
    ek_release(ek_val_ref(t2));
    ek_release(ek_val_ref(b));
}

/* A Bytes value keys its bytes under the default pair, and a String
   and a Bytes value over one byte sequence stay apart. */
static void dict_bytes_key_by_content(void)
{
    ek_obj *k1 = ek_str_from_cstr("k");
    ek_obj *b1 = ek_bytes_new("k", 1);
    ek_obj *b2 = ek_bytes_new("k", 1);
    ek_val d0 = ek_dict_new(ek_key_eq, ek_key_hash);
    ek_val d1 = ek_dict_set(d0, ek_val_ref(b1), ek_val_int(1));
    ek_val d2 = ek_dict_set(d1, ek_val_ref(b2), ek_val_int(2));
    ek_val out = ek_val_unit();

    EK_CHECK_U64(ek_dict_len(d2), 1); /* the second spelling overwrote */
    EK_CHECK(ek_dict_get(d2, ek_val_ref(b1), &out) && out.as.i == 2);
    EK_CHECK(!ek_dict_get(d2, ek_val_ref(k1), &out));
    ek_release(d2);
    ek_release(d1);
    ek_release(d0);
    ek_release(ek_val_ref(k1));
    ek_release(ek_val_ref(b1));
    ek_release(ek_val_ref(b2));
}

/* A container born without hooks refuses a reference key its own
   family cannot answer: the process aborts with the reason. */

#define ABORT_TEXT_CAP 128

typedef struct {
    int status;
    char text[ABORT_TEXT_CAP];
} abort_run;

static void read_err(int fd, char *buf, size_t cap)
{
    size_t len = 0;
    ssize_t n;

    while (len + 1 < cap) {
        n = read(fd, buf + len, cap - 1);
        if (n <= 0)
            break;
        len += (size_t)n;
    }
    buf[len] = '\0';
}

/* Runs body in a child whose standard error is the pipe, then records
   the wait status and everything the child wrote. */
static void run_abort(void (*body)(void), abort_run *run)
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
    read_err(fds[0], run->text, ABORT_TEXT_CAP);
    close(fds[0]);
    waitpid(pid, &run->status, 0);
}

static void body_dict_insert_without_eq(void)
{
    ek_val d = ek_dict_new(NULL, ek_key_hash);
    ek_val a = pair_of(&plain_info, 1, 1);

    ek_dict_set(d, a, ek_val_int(1));
}

static void body_dict_lookup_without_hash(void)
{
    ek_val d = ek_dict_new(pair_eq, NULL);
    ek_val a = pair_of(&plain_info, 1, 1);
    ek_val out = ek_val_unit();

    d = ek_dict_set(d, ek_val_ref(ek_str_from_cstr("k")),
                    ek_val_int(1));
    ek_dict_get(d, a, &out);
}

static void body_set_insert_without_hooks(void)
{
    ek_val s = ek_set_new(NULL, NULL);
    ek_val a = pair_of(&plain_info, 1, 1);

    ek_set_insert(s, a);
}

static void unhooked_reference_keys_abort(void)
{
    abort_run run;

    run_abort(body_dict_insert_without_eq, &run);
    EK_CHECK(WIFSIGNALED(run.status));
    EK_CHECK_U64(WTERMSIG(run.status), SIGABRT);
    EK_CHECK_STR(run.text, "abort: the dictionary key carries no equality "
                           "hook\n");

    run_abort(body_dict_lookup_without_hash, &run);
    EK_CHECK(WIFSIGNALED(run.status));
    EK_CHECK_U64(WTERMSIG(run.status), SIGABRT);
    EK_CHECK_STR(run.text, "abort: the dictionary key carries no hash "
                           "hook\n");

    run_abort(body_set_insert_without_hooks, &run);
    EK_CHECK(WIFSIGNALED(run.status));
    EK_CHECK_U64(WTERMSIG(run.status), SIGABRT);
    EK_CHECK_STR(run.text, "abort: the set element carries no hash hook\n");
}

int main(void)
{
    hooks_answer_over_the_layout();
    text_keys_stay_byte_based();
    set_text_hooks();
    dict_bytes_key_by_content();
    unhooked_reference_keys_abort();
    return ek_test_report("rt/hooks");
}
