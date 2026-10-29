#define _POSIX_C_SOURCE 200809L

#include "harness.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ekort.h"

/* The Out capability bundle: a struct record holding the one write
   closure, whose call sends the String argument's bytes to standard
   output without the terminator, and whose release runs with the
   bundle's drop. The redirect swaps the descriptor under the stream,
   so the suite's own output survives it. */

/* The bundle layout the compiler's generated one-field records share:
   the object header, then the write closure. */
typedef struct {
    ek_obj obj;
    ek_val write;
} cap_out;

static ek_val bundle_write(ek_val bundle)
{
    return ((cap_out *)bundle.as.ref)->write;
}

/* Points descriptor 1 at path and returns the saved descriptor, or -1
   with stdout untouched. */
static int redirect_stdout(const char *path)
{
    int saved = dup(STDOUT_FILENO);
    int fd;

    fflush(stdout);
    if (saved < 0) {
        return -1;
    }
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        close(saved);
        return -1;
    }
    if (dup2(fd, STDOUT_FILENO) < 0) {
        close(fd);
        close(saved);
        return -1;
    }
    close(fd);
    return saved;
}

/* Pushes the stream's buffer into the redirect, then puts descriptor 1
   back. */
static void restore_stdout(int saved)
{
    fflush(stdout);
    dup2(saved, STDOUT_FILENO);
    close(saved);
}

/* The file's bytes, or -1 when it cannot be read. */
static int read_file(const char *path, char *buf, size_t cap)
{
    FILE *fp = fopen(path, "rb");
    size_t got;
    int err;

    if (fp == NULL) {
        return -1;
    }
    got = fread(buf, 1, cap - 1, fp);
    err = ferror(fp);
    buf[got] = '\0';
    fclose(fp);
    return err != 0 ? -1 : (int)got;
}

static void bundle_structure(void)
{
    ek_val b = ek_cap_out_stdout();
    ek_val w;

    EK_CHECK(ek_val_is_ref(b));
    EK_CHECK_U64(b.as.ref->info->kind, EK_KIND_STRUCT);
    EK_CHECK_U64(b.as.ref->rc, 1); /* born with the caller's one ref */
    w = bundle_write(b);
    EK_CHECK(ek_val_is_ref(w));
    EK_CHECK_U64(w.as.ref->info->kind, EK_KIND_CLOSURE);
    EK_CHECK_U64(w.as.ref->rc, 1); /* the bundle's own reference */
    ek_release(b);
}

static void drop_releases_the_write(void)
{
    ek_val b = ek_cap_out_stdout();
    ek_val w = bundle_write(b);

    ek_retain(w);
    EK_CHECK_U64(w.as.ref->rc, 2);
    ek_release(b);
    EK_CHECK_U64(w.as.ref->rc, 1); /* the bundle's drop released one */
    ek_release(w);
}

static void write_lands_on_stdout(void)
{
    ek_val b = ek_cap_out_stdout();
    ek_val w = bundle_write(b);
    ek_obj *s = ek_str_from_cstr("echo hi");
    ek_val args[1] = { ek_val_ref(s) };
    ek_val r;
    char buf[64];
    int saved = redirect_stdout("build/rt_cap_stdout.txt");

    if (saved < 0) {
        EK_CHECK(0);
        ek_release(ek_val_ref(s));
        ek_release(b);
        return;
    }
    r = ek_closure_call(w, args, 1);
    restore_stdout(saved);
    EK_CHECK(ek_val_is_unit(r));
    EK_CHECK(read_file("build/rt_cap_stdout.txt", buf, sizeof buf) == 7);
    EK_CHECK_STR(buf, "echo hi");
    ek_release(ek_val_ref(s));
    ek_release(b);
}

static void writes_keep_their_order(void)
{
    ek_val b = ek_cap_out_stdout();
    ek_val w = bundle_write(b);
    ek_obj *one = ek_str_from_cstr("one");
    ek_obj *two = ek_str_from_cstr("two");
    ek_val args[1];
    char buf[64];
    int saved = redirect_stdout("build/rt_cap_order.txt");

    if (saved < 0) {
        EK_CHECK(0);
        ek_release(ek_val_ref(one));
        ek_release(ek_val_ref(two));
        ek_release(b);
        return;
    }
    args[0] = ek_val_ref(one);
    ek_closure_call(w, args, 1);
    args[0] = ek_val_ref(two);
    ek_closure_call(w, args, 1);
    restore_stdout(saved);
    EK_CHECK(read_file("build/rt_cap_order.txt", buf, sizeof buf) == 6);
    EK_CHECK_STR(buf, "onetwo");
    ek_release(ek_val_ref(one));
    ek_release(ek_val_ref(two));
    ek_release(b);
}

int main(void)
{
    if (mkdir("build", 0777) != 0 && errno != EEXIST) {
        fprintf(stderr, "rt/cap: cannot create build/\n");
        return 1;
    }
    bundle_structure();
    drop_releases_the_write();
    write_lands_on_stdout();
    writes_keep_their_order();
    return ek_test_report("rt/cap");
}
