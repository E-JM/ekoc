#include "harness.h"

#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ekort.h"

/* The key hooks a container carries: a dictionary or set born over an
   eq and hash pair keys its domain through them, so equal-but-distinct
   objects are one key, unequal ones hash apart, and the identity holds
   across every lookup within the run. Every copy-on-insert step clones
   the carried pair, so a build chain keys by one discipline. The
   default pair keys the scalar, Float, and text domains itself and
   falls back to the family's own hooks, the object's address when the
   family has none; a reference key the carried pair cannot answer
   aborts the process. */

/* Two fake families over the same two-slot layout: the hooks compare
   and mix the slots, the identity separating the families stays with
   the typeinfo pointer. */
typedef struct {
    ek_obj obj;
    ek_val x;
    ek_val y;
} ek_pair;

static const ek_typeinfo pair_a_info;
static const ek_typeinfo pair_b_info;

static int pair_eq(ek_val a, ek_val b)
{
    const ek_pair *pa = (const ek_pair *)a.as.ref;
    const ek_pair *pb = (const ek_pair *)b.as.ref;

    return a.as.ref->info == b.as.ref->info
        && pa->x.as.i == pb->x.as.i
        && pa->y.as.i == pb->y.as.i;
}

static unsigned long pair_hash(ek_val v)
{
    const ek_pair *p = (const ek_pair *)v.as.ref;
    unsigned long h = 14695981039346656037ULL;

    h = (h ^ (unsigned long)v.as.ref->info) * 1099511628211ULL;
    h = (h ^ (unsigned long)p->x.as.i) * 1099511628211ULL;
    h = (h ^ (unsigned long)p->y.as.i) * 1099511628211ULL;
    return h;
}

static const ek_typeinfo pair_a_info = { EK_KIND_STRUCT, NULL,
                                         pair_eq, pair_hash };
static const ek_typeinfo pair_b_info = { EK_KIND_STRUCT, NULL,
                                         pair_eq, pair_hash };
static const ek_typeinfo plain_info = { EK_KIND_STRUCT, NULL, NULL, NULL };

/* A pair object the caller owns and releases with one reference. */
static ek_val pair_of(const ek_typeinfo *info, int64_t x, int64_t y)
{
    ek_pair *p = calloc(1, sizeof *p);

    p->obj.info = info;
    p->obj.rc = 1;
    p->x = ek_val_int(x);
    p->y = ek_val_int(y);
    return ek_val_ref(&p->obj);
}

/* A boxed Double over the hooks a Double-carrying family passes: the
   equality is the Float rules, so the two zeros are one value and a
   NaN equals nothing, not even itself. The hash normalizes the zeros
   the same way, so equal values hash equal. */
typedef struct {
    ek_obj obj;
    double d;
} ek_boxed;

static int boxed_eq(ek_val a, ek_val b)
{
    const ek_boxed *pa = (const ek_boxed *)a.as.ref;
    const ek_boxed *pb = (const ek_boxed *)b.as.ref;

    return a.as.ref->info == b.as.ref->info && pa->d == pb->d;
}

static unsigned long boxed_hash(ek_val v)
{
    const ek_boxed *p = (const ek_boxed *)v.as.ref;
    unsigned long h = 14695981039346656037ULL;
    unsigned long bits;
    double d = p->d == 0.0 ? 0.0 : p->d;

    memcpy(&bits, &d, sizeof bits);
    return (h ^ bits) * 1099511628211ULL;
}

static const ek_typeinfo boxed_info = { EK_KIND_STRUCT, NULL,
                                        boxed_eq, boxed_hash };

/* A boxed Double the caller owns and releases with one reference. */
static ek_val boxed_of(double d)
{
    ek_boxed *p = calloc(1, sizeof *p);

    p->obj.info = &boxed_info;
    p->obj.rc = 1;
    p->d = d;
    return ek_val_ref(&p->obj);
}

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

static void dict_keys_structurally(void)
{
    ek_val d0 = ek_dict_new(ek_key_eq, ek_key_hash);
    ek_val a = pair_of(&pair_a_info, 4, 5);
    ek_val b = pair_of(&pair_a_info, 4, 5);
    ek_val c = pair_of(&pair_a_info, 4, 6);
    ek_val d1 = ek_dict_set(d0, a, ek_val_int(7));
    ek_val d2 = ek_dict_set(d1, b, ek_val_int(9));
    ek_val out = ek_val_unit();

    EK_CHECK_U64(ek_dict_len(d1), 1);
    EK_CHECK_U64(ek_dict_len(d2), 1); /* b overwrote a */
    EK_CHECK(ek_dict_get(d2, b, &out));
    EK_CHECK_U64(out.as.i, 9);
    EK_CHECK(ek_dict_get(d2, a, &out)); /* the same key either spelling */
    EK_CHECK_U64(out.as.i, 9);
    EK_CHECK(!ek_dict_get(d2, c, &out));
    EK_CHECK_U64(a.as.ref->rc, 4); /* the caller's, each version's trie's,
                                      and the order vector's */
    ek_release(d2);
    EK_CHECK_U64(a.as.ref->rc, 3); /* d1's trie and order vector keep
                                      theirs */
    ek_release(d1);
    EK_CHECK_U64(a.as.ref->rc, 1);
    ek_release(d0);
    ek_release(a);
    ek_release(b);
    ek_release(c);
}

static void dict_distinct_keys_coexist(void)
{
    ek_val a = pair_of(&pair_a_info, 1, 1);
    ek_val b = pair_of(&pair_a_info, 2, 2);
    ek_val d0 = ek_dict_new(ek_key_eq, ek_key_hash);
    ek_val d1 = ek_dict_set(d0, a, ek_val_int(1));
    ek_val d2 = ek_dict_set(d1, b, ek_val_int(2));
    ek_val out = ek_val_unit();

    EK_CHECK_U64(ek_dict_len(d2), 2);
    EK_CHECK(ek_dict_get(d2, a, &out) && out.as.i == 1);
    EK_CHECK(ek_dict_get(d2, b, &out) && out.as.i == 2);
    ek_release(d2);
    ek_release(d1);
    ek_release(d0);
    ek_release(a);
    ek_release(b);
}

/* The typeinfo pointer is the precheck: two families over one layout
   stay apart even when the fields read equal. */
static void dict_separates_the_families(void)
{
    ek_val a = pair_of(&pair_a_info, 3, 4);
    ek_val b = pair_of(&pair_b_info, 3, 4);
    ek_val d0 = ek_dict_new(ek_key_eq, ek_key_hash);
    ek_val d1 = ek_dict_set(d0, a, ek_val_int(1));
    ek_val d2 = ek_dict_set(d1, b, ek_val_int(2));
    ek_val out = ek_val_unit();

    EK_CHECK(!pair_eq(a, b));
    EK_CHECK_U64(ek_dict_len(d2), 2);
    EK_CHECK(ek_dict_get(d2, a, &out) && out.as.i == 1);
    EK_CHECK(ek_dict_get(d2, b, &out) && out.as.i == 2);
    ek_release(d2);
    ek_release(d1);
    ek_release(d0);
    ek_release(a);
    ek_release(b);
}

/* A family without hooks keys on the object's address, the rule the
   default pair keeps for the families it cannot walk. */
static void dict_keeps_the_address_rule_without_hooks(void)
{
    ek_val a = pair_of(&plain_info, 8, 9);
    ek_val b = pair_of(&plain_info, 8, 9);
    ek_val d0 = ek_dict_new(ek_key_eq, ek_key_hash);
    ek_val d1 = ek_dict_set(d0, a, ek_val_int(1));
    ek_val d2 = ek_dict_set(d1, b, ek_val_int(2));
    ek_val out = ek_val_unit();

    EK_CHECK_U64(ek_dict_len(d2), 2); /* distinct objects, two keys */
    EK_CHECK(ek_dict_get(d2, a, &out) && out.as.i == 1);
    ek_release(d2);
    ek_release(d1);
    ek_release(d0);
    ek_release(a);
    ek_release(b);
}

static void set_keys_structurally(void)
{
    ek_val a = pair_of(&pair_a_info, 6, 7);
    ek_val b = pair_of(&pair_a_info, 6, 7);
    ek_val c = pair_of(&pair_a_info, 9, 9);
    ek_val s0 = ek_set_new(ek_key_eq, ek_key_hash);
    ek_val s1 = ek_set_insert(s0, a);
    ek_val s2 = ek_set_insert(s1, b);
    ek_val s3 = ek_set_insert(s2, c);
    ek_val other = pair_of(&pair_b_info, 6, 7);

    EK_CHECK_U64(ek_set_len(s1), 1);
    EK_CHECK_U64(ek_set_len(s2), 1); /* b is a */
    EK_CHECK(ek_set_contains(s2, a));
    EK_CHECK(ek_set_contains(s2, b));
    EK_CHECK_U64(ek_set_len(s3), 2);
    EK_CHECK(!ek_set_contains(s3, other)); /* another family never */
    EK_CHECK_U64(a.as.ref->rc, 3); /* the caller's, s1's and s3's */
    ek_release(s3);
    EK_CHECK_U64(a.as.ref->rc, 2);
    ek_release(s2); /* s2 is s1: the no-op insert returned the original */
    EK_CHECK_U64(a.as.ref->rc, 2);
    ek_release(s1);
    EK_CHECK_U64(a.as.ref->rc, 1);
    ek_release(s0);
    ek_release(other);
    ek_release(a);
    ek_release(b);
    ek_release(c);
}

static void set_dedups_over_the_hooks(void)
{
    ek_val a = pair_of(&pair_a_info, 1, 2);
    ek_val b = pair_of(&pair_a_info, 1, 2);
    ek_val c = pair_of(&pair_a_info, 1, 3);
    ek_val other = pair_of(&pair_b_info, 1, 2);
    ek_val last = ek_cons(c, ek_nil_val());
    ek_val rest = ek_cons(b, last);
    ek_val list = ek_cons(a, rest);
    ek_val s = ek_set_of(list, ek_key_eq, ek_key_hash);
    ek_val less = ek_set_remove(s, b);

    /* each fold's intermediate dies once the next cell holds its own */
    ek_release(last);
    ek_release(rest);
    EK_CHECK_U64(ek_set_len(s), 2); /* the first occurrence keeps */
    EK_CHECK(ek_set_contains(s, b));
    EK_CHECK(!ek_set_contains(s, other));
    EK_CHECK(less.as.ref != s.as.ref);
    EK_CHECK_U64(ek_set_len(less), 1);
    ek_release(less);
    ek_release(s);
    ek_release(list);
    ek_release(other);
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

/* The Float key rules the element domain pins: 0.0 and -0.0 are one
   element and a NaN element never matches, not even itself. */
static void set_float_elements_follow_ieee_equality(void)
{
    ek_val s0 = ek_set_new(ek_key_eq, ek_key_hash);
    ek_val s1 = ek_set_insert(s0, ek_val_float(0.0));
    ek_val s2 = ek_set_insert(s1, ek_val_float(-0.0));
    ek_val s3 = ek_set_insert(s2, ek_val_float(1.5));
    ek_val s4 = ek_set_insert(s3, ek_val_float(0.0 / 0.0));

    EK_CHECK_U64(ek_set_len(s2), 1); /* -0.0 found 0.0 */
    EK_CHECK(ek_set_contains(s2, ek_val_float(-0.0)));
    EK_CHECK_U64(ek_set_len(s4), 3);
    EK_CHECK(!ek_set_contains(s4, ek_val_float(0.0 / 0.0)));
    ek_release(s4);
    ek_release(s3);
    ek_release(s2);
    ek_release(s1);
    ek_release(s0);
}

/* The same Float key rules for the dictionary: 0.0 and -0.0 are one
   key under either spelling, and a NaN key is never itself, so its
   insert returns the original unchanged and no lookup finds it. */
static void dict_float_keys_follow_ieee_equality(void)
{
    ek_val d0 = ek_dict_new(ek_key_eq, ek_key_hash);
    ek_val d1 = ek_dict_set(d0, ek_val_float(-0.0), ek_val_int(1));
    ek_val d2 = ek_dict_set(d1, ek_val_float(0.0), ek_val_int(2));
    ek_val d3 = ek_dict_set(d2, ek_val_float(1.5), ek_val_int(3));
    ek_val nan = ek_val_float(0.0 / 0.0);
    ek_val d4 = ek_dict_set(d3, nan, ek_val_int(4));
    ek_val e0 = ek_dict_new(ek_key_eq, ek_key_hash);
    ek_val e1 = ek_dict_set(e0, nan, ek_val_int(5));
    ek_val out = ek_val_unit();

    EK_CHECK_U64(ek_dict_len(d1), 1);
    EK_CHECK(ek_dict_get(d1, ek_val_float(0.0), &out)); /* found under 0.0 */
    EK_CHECK_U64(out.as.i, 1);
    EK_CHECK_U64(ek_dict_len(d2), 1); /* 0.0 found -0.0 and replaced it */
    EK_CHECK(ek_dict_get(d2, ek_val_float(-0.0), &out));
    EK_CHECK_U64(out.as.i, 2);
    EK_CHECK(d4.as.ref == d3.as.ref); /* the NaN insert returned the original */
    EK_CHECK_U64(d3.as.ref->rc, 2); /* the caller's and the alias's */
    EK_CHECK_U64(ek_dict_len(d4), 2);
    EK_CHECK(!ek_dict_get(d4, nan, &out));
    EK_CHECK(ek_dict_get(d4, ek_val_float(1.5), &out) && out.as.i == 3);
    EK_CHECK(e1.as.ref == e0.as.ref); /* a NaN cannot even start a table */
    EK_CHECK_U64(ek_dict_len(e1), 0);
    EK_CHECK(!ek_dict_get(e1, nan, &out));
    ek_release(e1);
    ek_release(e0);
    ek_release(d4);
    ek_release(d3);
    ek_release(d2);
    ek_release(d1);
    ek_release(d0);
}

/* The self-identity the NaN rule needs also runs through a carried
   pair: the insert consults the carried eq for the key against itself,
   so a boxed Double carrying a NaN inserts nothing and the insert
   returns the original, while the same pair still keys its equal
   boxes as one. */
static void dict_carried_pair_keeps_the_nan_rule(void)
{
    ek_val a = boxed_of(2.5);
    ek_val b = boxed_of(2.5);
    ek_val nan = boxed_of(0.0 / 0.0);
    ek_val d0 = ek_dict_new(boxed_eq, boxed_hash);
    ek_val d1 = ek_dict_set(d0, a, ek_val_int(1));
    ek_val d2 = ek_dict_set(d1, b, ek_val_int(2));
    ek_val d3 = ek_dict_set(d2, nan, ek_val_int(3));
    ek_val out = ek_val_unit();

    EK_CHECK_U64(ek_dict_len(d2), 1); /* the carried eq found b equal to a */
    EK_CHECK(ek_dict_get(d2, a, &out) && out.as.i == 2);
    EK_CHECK(d3.as.ref == d2.as.ref); /* the NaN box is not even itself */
    EK_CHECK_U64(d2.as.ref->rc, 2); /* the caller's and the alias's */
    EK_CHECK_U64(ek_dict_len(d3), 1);
    EK_CHECK(!ek_dict_get(d3, nan, &out));
    ek_release(d3);
    ek_release(d2);
    ek_release(d1);
    ek_release(d0);
    ek_release(nan);
    ek_release(b);
    ek_release(a);
}

/* The carried pair travels with every copy-on-insert step: a build
   chain over the pair hooks keeps keying structurally after each
   insert, and a merge reads through the left side's pair. */
static void dict_set_clones_the_pair(void)
{
    ek_val d0 = ek_dict_new(pair_eq, pair_hash);
    ek_val right = ek_dict_new(ek_key_eq, ek_key_hash);
    ek_val a = pair_of(&pair_a_info, 1, 4);
    ek_val b = pair_of(&pair_a_info, 1, 4);
    ek_val out = ek_val_unit();
    ek_val d1 = ek_dict_set(d0, a, ek_val_int(1));
    ek_val d2 = ek_dict_set(d1, b, ek_val_int(2));
    ek_val joined = ek_dict_merge(d2, right);

    EK_CHECK_U64(ek_dict_len(d2), 1); /* the clone found b equal to a */
    EK_CHECK(ek_dict_get(d2, b, &out) && out.as.i == 2);
    EK_CHECK_U64(ek_dict_len(joined), 1);
    EK_CHECK(ek_dict_get(joined, b, &out) && out.as.i == 2);
    ek_release(joined);
    ek_release(d2);
    ek_release(d1);
    ek_release(d0);
    ek_release(right);
    ek_release(a);
    ek_release(b);
}

/* The element pair clones the same way across an insert chain. */
static void set_insert_clones_the_pair(void)
{
    ek_val s0 = ek_set_new(pair_eq, pair_hash);
    ek_val a = pair_of(&pair_a_info, 2, 5);
    ek_val b = pair_of(&pair_a_info, 2, 5);
    ek_val s1 = ek_set_insert(s0, a);
    ek_val s2 = ek_set_insert(s1, b);

    EK_CHECK_U64(ek_set_len(s2), 1); /* the clone found b equal to a */
    EK_CHECK(ek_set_contains(s2, b));
    ek_release(s2);
    ek_release(s1);
    ek_release(s0);
    ek_release(a);
    ek_release(b);
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
    dict_keys_structurally();
    dict_distinct_keys_coexist();
    dict_separates_the_families();
    dict_keeps_the_address_rule_without_hooks();
    set_keys_structurally();
    set_dedups_over_the_hooks();
    text_keys_stay_byte_based();
    set_text_hooks();
    set_float_elements_follow_ieee_equality();
    dict_float_keys_follow_ieee_equality();
    dict_carried_pair_keeps_the_nan_rule();
    dict_set_clones_the_pair();
    set_insert_clones_the_pair();
    dict_bytes_key_by_content();
    unhooked_reference_keys_abort();
    return ek_test_report("rt/hooks");
}
