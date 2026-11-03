#define _POSIX_C_SOURCE 200809L

#include "harness.h"
#include "capident.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ekort.h"

/* The file family: the Fs bundle over the working directory and the
   File handles over the runtime's table. The suite works on real files
   under build/ and reads the results the way generated code does --
   the Ok and Err payloads by the layout contract, the reasons by the
   identity the unit defines. */

/* The bundle's member closures, by the record's field order. */
static ek_val member(ek_val bundle, size_t i)
{
    return ((ek_val *)bundle.as.ref)[i + 1];
}

/* The Ok payload, borrowed. */
static ek_val ok_payload(ek_val r)
{
    return ((ek_val *)r.as.ref)[1];
}

/* The Ok payload with a reference of the caller's own: the value
   outlives the result. */
static ek_val owned_payload(ek_val r)
{
    ek_val v = ok_payload(r);

    ek_retain(v);
    return v;
}

static int is_ok(ek_val r)
{
    return r.as.ref->info == &ekT_std_Result_Ok;
}

static const ek_typeinfo *err_kind(ek_val r)
{
    return ((ek_val *)r.as.ref)[1].as.ref->info;
}

static ek_val call1(ek_val f, ek_val a)
{
    ek_val args[1];

    args[0] = a;
    return ek_closure_call(f, args, 1);
}

static ek_val call2(ek_val f, ek_val a, ek_val b)
{
    ek_val args[2];

    args[0] = a;
    args[1] = b;
    return ek_closure_call(f, args, 2);
}

static ek_val s(const char *t)
{
    return ek_val_ref(ek_str_from_cstr(t));
}

/* Releases an object the runtime handed over. */
static void drop(ek_obj *o)
{
    ek_release(ek_val_ref(o));
}

/* An OpenMode value the way a program builds one: a fieldless variant
   object over its identity. */
static ek_val capident_mode(const ek_typeinfo *id)
{
    ek_obj *o = calloc(1, sizeof *o);

    if (o == NULL) {
        exit(1);
    }
    o->info = id;
    o->rc = 1;
    return ek_val_ref(o);
}

/* The result payload as text, released; NULL when the call failed. */
static const char *read_text(ek_val r)
{
    static char buf[64];
    ek_val t;

    if (!is_ok(r)) {
        return NULL;
    }
    t = owned_payload(r);
    snprintf(buf, sizeof buf, "%s", ek_str_cstr(t.as.ref));
    ek_release(t);
    return buf;
}

static void bundle_structure(void)
{
    ek_val b = ek_cap_fs_cwd();
    size_t i;

    EK_CHECK(ek_val_is_ref(b));
    EK_CHECK_U64(b.as.ref->info->kind, EK_KIND_STRUCT);
    for (i = 0; i < 16; i++) {
        ek_val m = member(b, i);

        EK_CHECK(ek_val_is_ref(m));
        EK_CHECK_U64(m.as.ref->info->kind, EK_KIND_CLOSURE);
        EK_CHECK_U64(m.as.ref->rc, 1);
    }
    ek_release(b);
}

static void open_read_missing(void)
{
    ek_val b = ek_cap_fs_cwd();
    ek_val r = call2(member(b, 0), s("build/no_such_file_here"),
                     capident_mode(&ekT_sys_OpenMode_Read));

    EK_CHECK(ek_val_is_ref(r));
    EK_CHECK(!is_ok(r));
    EK_CHECK(err_kind(r) == &ekT_sys_IoError_NotFound);
    ek_release(r);
    ek_release(b);
}

static void write_read_round_trip(void)
{
    ek_val b = ek_cap_fs_cwd();
    ek_val path = s("build/rt_fs_round.txt");
    ek_val r = call2(member(b, 4), path, s("round body"));
    const char *got;

    EK_CHECK(is_ok(r));
    ek_release(r);
    got = read_text(r = call1(member(b, 3), path));
    EK_CHECK_STR(got, "round body");
    ek_release(r);
    r = call1(member(b, 6), path);
    EK_CHECK(is_ok(r));
    if (is_ok(r)) {
        ek_val bs = ok_payload(r);

        EK_CHECK_U64(ek_bytes_len(bs.as.ref), 10);
        EK_CHECK(memcmp(ek_bytes_data(bs.as.ref), "round body", 10) == 0);
        ek_release(bs);
    }
    ek_release(r);
    ek_release(path);
    ek_release(b);
}

static void append_and_new_modes(void)
{
    ek_val b = ek_cap_fs_cwd();
    ek_val path = s("build/rt_fs_modes.txt");
    ek_val r;
    const char *got;

    r = call2(member(b, 4), path, s("one"));
    EK_CHECK(is_ok(r));
    ek_release(r);
    r = call2(member(b, 5), path, s(" two"));
    EK_CHECK(is_ok(r));
    ek_release(r);
    got = read_text(r = call1(member(b, 3), path));
    EK_CHECK_STR(got, "one two");
    ek_release(r);
    r = call2(member(b, 0), path, capident_mode(&ekT_sys_OpenMode_New));
    EK_CHECK(!is_ok(r));
    EK_CHECK(err_kind(r) == &ekT_sys_IoError_Exists);
    ek_release(r);
    unlink("build/rt_fs_new.txt");
    r = call2(member(b, 0), s("build/rt_fs_new.txt"),
              capident_mode(&ekT_sys_OpenMode_New));
    EK_CHECK(is_ok(r));
    drop(ok_payload(r).as.ref);
    ek_release(r);
    ek_release(path);
    ek_release(b);
}

/* A handle writes, reads its own bytes back by count, and seeks. */
static void handle_read_write_seek(void)
{
    ek_val b = ek_cap_fs_cwd();
    ek_val path = s("build/rt_fs_handle.txt");
    ek_val r = call2(member(b, 0), path,
                     capident_mode(&ekT_sys_OpenMode_Write));
    ek_val f;
    ek_obj *out = NULL;
    int err = 0;
    ek_obj *detail = NULL;
    ek_obj *body = ek_str_from_cstr("alpha\nbeta");

    (void)detail;

    EK_CHECK(is_ok(r));
    f = owned_payload(r);
    ek_release(r);
    EK_CHECK_U64(ek_file_write(f, body, &err, &detail), 1);
    EK_CHECK_U64(ek_file_flush(f, &err, &detail), 1);
    EK_CHECK_U64(ek_file_close(f, &err, &detail), 1);
    ek_release(f);

    /* the reads go through a handle reopened for update */
    r = call2(member(b, 0), path, capident_mode(&ekT_sys_OpenMode_Update));
    EK_CHECK(is_ok(r));
    f = owned_payload(r);
    ek_release(r);
    EK_CHECK_U64(ek_file_seek(f, ek_val_uint(6), &err, &detail), 1);
    EK_CHECK_U64(ek_file_read(f, 4, &out, &err, &detail), 1);
    if (out != NULL) {
        EK_CHECK_STR(ek_str_cstr(out), "beta");
        drop(out);
    }
    EK_CHECK_U64(ek_file_seek(f, ek_val_uint(100), &err, &detail), 0);
    EK_CHECK_U64(err, EK_IO_INVALID_DATA);
    EK_CHECK_U64(ek_file_seek(f, ek_val_uint(10), &err, &detail), 1);
    EK_CHECK_U64(ek_file_read_bytes(f, 3, &out, &err, &detail), 1);
    if (out != NULL) {
        EK_CHECK_U64(ek_bytes_len(out), 0); /* at end of input */
        drop(out);
    }
    EK_CHECK_U64(ek_file_close(f, &err, &detail), 1);
    drop(body);
    ek_release(f);
    ek_release(path);
    ek_release(b);
}

/* read_line hands out the lines without their newlines and reports end
   of input as success with no line. */
static void handle_lines(void)
{
    ek_val b = ek_cap_fs_cwd();
    ek_val path = s("build/rt_fs_lines.txt");
    ek_val r = call2(member(b, 0), path,
                     capident_mode(&ekT_sys_OpenMode_Write));
    ek_val f;
    ek_obj *out = NULL;
    int err = 0;
    ek_obj *detail = NULL;

    EK_CHECK(is_ok(r));
    f = owned_payload(r);
    ek_release(r);
    EK_CHECK_U64(ek_file_write(f, ek_str_from_cstr("alpha\nbeta"), &err,
                               &detail),
                 1);
    EK_CHECK_U64(ek_file_close(f, &err, &detail), 1);

    r = call2(member(b, 0), path, capident_mode(&ekT_sys_OpenMode_Read));
    EK_CHECK(is_ok(r));
    f = owned_payload(r);
    ek_release(r);
    EK_CHECK_U64(ek_file_read_line(f, &out, &err, &detail), 1);
    EK_CHECK(out != NULL && strcmp(ek_str_cstr(out), "alpha") == 0);
    if (out != NULL) {
        drop(out);
    }
    EK_CHECK_U64(ek_file_read_line(f, &out, &err, &detail), 1);
    EK_CHECK(out != NULL && strcmp(ek_str_cstr(out), "beta") == 0);
    if (out != NULL) {
        drop(out);
    }
    EK_CHECK_U64(ek_file_read_line(f, &out, &err, &detail), 1);
    EK_CHECK(out == NULL);
    EK_CHECK_U64(ek_file_close(f, &err, &detail), 1);
    ek_release(f);
    ek_release(path);
    ek_release(b);
}

/* The text gate: bytes that are not well-formed UTF-8 report
   InvalidData on the text read and pass through the byte read. */
static void text_gate(void)
{
    ek_val b = ek_cap_fs_cwd();
    ek_val path = s("build/rt_fs_gate.bin");
    ek_val r = call2(member(b, 7), path,
                     ek_val_ref(ek_bytes_new("\xed\xa0\x80", 3)));
    ek_val f;
    ek_obj *out = NULL;
    int err = 0;
    ek_obj *detail = NULL;

    EK_CHECK(is_ok(r));
    ek_release(r);
    unlink("build/rt_fs_gate.bin");
    r = call2(member(b, 7), path,
              ek_val_ref(ek_bytes_new("\xed\xa0\x80", 3)));
    EK_CHECK(is_ok(r));
    ek_release(r);
    r = call2(member(b, 0), path, capident_mode(&ekT_sys_OpenMode_Read));
    EK_CHECK(is_ok(r));
    f = owned_payload(r);
    ek_release(r);
    EK_CHECK_U64(ek_file_read(f, 8, &out, &err, &detail), 0);
    EK_CHECK_U64(err, EK_IO_INVALID_DATA);
    EK_CHECK_U64(ek_file_close(f, &err, &detail), 1);
    ek_release(f);
    r = call2(member(b, 0), path, capident_mode(&ekT_sys_OpenMode_Read));
    EK_CHECK(is_ok(r));
    f = owned_payload(r);
    ek_release(r);
    EK_CHECK_U64(ek_file_read_bytes(f, 8, &out, &err, &detail), 1);
    EK_CHECK(out != NULL && ek_bytes_len(out) == 3);
    if (out != NULL) {
        drop(out);
    }
    EK_CHECK_U64(ek_file_close(f, &err, &detail), 1);
    ek_release(f);
    ek_release(path);
    ek_release(b);
}

/* Generations: the stale copy reads as closed, a second close
   included, and the reused slot's new handle works. */
static void generations(void)
{
    ek_val b = ek_cap_fs_cwd();
    ek_val path = s("build/rt_fs_gen.txt");
    ek_val r = call2(member(b, 0), path,
                     capident_mode(&ekT_sys_OpenMode_Write));
    ek_val f;
    ek_val f2;
    int err = 0;
    ek_obj *detail = NULL;
    ek_obj *out = NULL;

    EK_CHECK(is_ok(r));
    f = owned_payload(r);
    ek_release(r);
    EK_CHECK_U64(ek_file_close(f, &err, &detail), 1);
    EK_CHECK_U64(ek_file_close(f, &err, &detail), 0);
    EK_CHECK_U64(err, EK_IO_CLOSED);
    EK_CHECK_U64(ek_file_read_line(f, &out, &err, &detail), 0);
    EK_CHECK_U64(err, EK_IO_CLOSED);

    r = call2(member(b, 0), path, capident_mode(&ekT_sys_OpenMode_Write));
    EK_CHECK(is_ok(r));
    f2 = owned_payload(r);
    ek_release(r);
    EK_CHECK_U64(ek_file_write(f2, ek_str_from_cstr("x"), &err, &detail), 1);
    EK_CHECK_U64(ek_file_read_line(f, &out, &err, &detail), 0);
    EK_CHECK_U64(err, EK_IO_CLOSED);
    EK_CHECK_U64(ek_file_close(f2, &err, &detail), 1);
    ek_release(f);
    ek_release(f2);
    ek_release(path);
    ek_release(b);
}

/* The with_file callback: it runs with the handle, the value comes
   back wrapped, and the handle closes on both paths. */
static ek_val write_inside(ek_val env, ek_val *args, size_t argc)
{
    int err = 0;
    ek_obj *detail = NULL;
    ek_val r;

    (void)env;
    if (argc != 1) {
        return ek_val_int(0 - 1);
    }
    r = ek_val_int(ek_file_write(args[0], ek_str_from_cstr("from with"), &err,
                                 &detail)
                       ? 7
                       : 0 - 1);
    if (detail != NULL) {
        ek_release(ek_val_ref(detail));
    }
    return r;
}

static void with_file_closes(void)
{
    ek_val b = ek_cap_fs_cwd();
    ek_val path = s("build/rt_fs_with.txt");
    ek_val cb = ek_closure_new(write_inside, ek_val_unit());
    ek_val args[3];
    ek_val out;

    args[0] = path;
    args[1] = capident_mode(&ekT_sys_OpenMode_Write);
    args[2] = cb;
    out = ek_closure_call(member(b, 1), args, 3);
    EK_CHECK(is_ok(out));
    if (is_ok(out)) {
        EK_CHECK_U64(ok_payload(out).as.i, 7);
    }
    ek_release(out);
    ek_release(cb);
    /* the handle closed with the scope: the file holds the bytes */
    EK_CHECK_U64(access("build/rt_fs_with.txt", F_OK), 0);
    {
        FILE *fp = fopen("build/rt_fs_with.txt", "rb");
        char buf[32];

        EK_CHECK(fp != NULL);
        if (fp != NULL) {
            size_t got = fread(buf, 1, sizeof buf - 1, fp);

            buf[got] = 0;
            EK_CHECK_STR(buf, "from with");
            fclose(fp);
        }
    }
    ek_release(path);
    ek_release(b);
}

/* The directory family: listing in order without the dot entries, one
   and many levels of creation, removal, and the rename. */
static void directory_family(void)
{
    ek_val b = ek_cap_fs_cwd();
    ek_val dir = s("build/rt_fs_dir");
    ek_val r;
    ek_val names;
    ek_val it;

    EK_CHECK(system("rm -rf build/rt_fs_dir") == 0);
    r = call1(member(b, 9), dir);
    EK_CHECK(is_ok(r));
    ek_release(r);
    r = call1(member(b, 9), dir);
    EK_CHECK(!is_ok(r));
    EK_CHECK(err_kind(r) == &ekT_sys_IoError_Exists);
    ek_release(r);
    r = call1(member(b, 10), s("build/rt_fs_dir/a/b"));
    EK_CHECK(is_ok(r));
    ek_release(r);

    EK_CHECK(system("touch build/rt_fs_dir/x.txt build/rt_fs_dir/a/m.txt")
            == 0);
    r = call1(member(b, 8), dir);
    EK_CHECK(is_ok(r));
    names = ok_payload(r);
    EK_CHECK_U64(ek_list_len(names), 2); /* a and x.txt, no dot entries */
    it = names;
    EK_CHECK(ek_val_is_cons(it));
    EK_CHECK_STR(ek_str_cstr(ek_cons_head(it.as.ref).as.ref), "a");
    it = ek_cons_tail(it.as.ref);
    EK_CHECK_STR(ek_str_cstr(ek_cons_head(it.as.ref).as.ref), "x.txt");
    ek_release(names);
    ek_release(r);

    r = call1(member(b, 8), s("build/rt_fs_dir/x.txt"));
    EK_CHECK(!is_ok(r));
    EK_CHECK(err_kind(r) == &ekT_sys_IoError_NotFile);
    ek_release(r);

    r = call2(member(b, 13), s("build/rt_fs_dir/x.txt"),
              s("build/rt_fs_dir/y.txt"));
    EK_CHECK(is_ok(r));
    ek_release(r);
    EK_CHECK(access("build/rt_fs_dir/y.txt", F_OK) == 0);
    ek_release(dir);
    ek_release(b);
}

/* The removal family: a directory is not a file, an empty directory
   goes, a non-empty one carries the Other reason. */
static void removal_family(void)
{
    ek_val b = ek_cap_fs_cwd();
    ek_val dir = s("build/rt_fs_dir");
    ek_val r;

    r = call1(member(b, 11), dir);
    EK_CHECK(!is_ok(r));
    EK_CHECK(err_kind(r) == &ekT_sys_IoError_NotFile);
    ek_release(r);
    r = call1(member(b, 12), s("build/rt_fs_dir/a/b"));
    EK_CHECK(is_ok(r));
    ek_release(r);
    r = call1(member(b, 12), dir);
    EK_CHECK(!is_ok(r)); /* still non-empty */
    EK_CHECK(err_kind(r) == &ekT_sys_IoError_Other);
    ek_release(r);
    ek_release(dir);
    ek_release(b);
}

/* The stat: the size, the directory flag, and the modification time as
   a Time value over the shared identity. */
static void stat_file_info(void)
{
    ek_val b = ek_cap_fs_cwd();
    ek_val r;
    ek_val info;
    ek_val r2;

    EK_CHECK(system("printf '12345' > build/rt_fs_stat.txt") == 0);
    r2 = call1(member(b, 14), s("build/rt_fs_stat.txt"));
    EK_CHECK(is_ok(r2));
    if (is_ok(r2)) {
        info = ok_payload(r2);
        EK_CHECK(info.as.ref->info == &ekT_sys_FileInfo);
        EK_CHECK_U64(((ek_val *)info.as.ref)[1].as.u, 5);
        EK_CHECK_U64(((ek_val *)info.as.ref)[2].as.ref->info,
                     (unsigned long)&ekT_time_Time_Time);
        EK_CHECK_U64(((ek_val *)info.as.ref)[3].as.i, 0);
        ek_release(info);
    }
    ek_release(r2);
    unlink("build/rt_fs_stat.txt");
    r = call1(member(b, 14), s("build/rt_fs_stat.txt"));
    EK_CHECK(!is_ok(r) && err_kind(r) == &ekT_sys_IoError_NotFound);
    ek_release(r);
    r = call1(member(b, 14), s("build"));
    EK_CHECK(is_ok(r));
    if (is_ok(r)) {
        info = ok_payload(r);
        EK_CHECK_U64(((ek_val *)info.as.ref)[3].as.i, 1);
        ek_release(info);
    }
    ek_release(r);
    ek_release(b);
}

int main(void)
{
    if (mkdir("build", 0777) != 0 && errno != EEXIST) {
        fprintf(stderr, "rt/fs: cannot create build/\n");
        return 1;
    }
    bundle_structure();
    open_read_missing();
    write_read_round_trip();
    append_and_new_modes();
    handle_read_write_seek();
    handle_lines();
    text_gate();
    generations();
    with_file_closes();
    directory_family();
    removal_family();
    stat_file_info();
    return ek_test_report("rt/fs");
}
