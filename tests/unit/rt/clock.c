#define _POSIX_C_SOURCE 200809L

#include "harness.h"
#include "capident.h"

#include <time.h>

#include "ekort.h"

/* The wall-clock bundle: the readings travel as the time companion's
   values over the shared identity, the payload being the nanosecond
   count behind the header; sleep waits through nanosleep and takes a
   non-positive duration back immediately. */

/* The bundle's member closures, in the record's field order. */
static ek_val member(ek_val bundle, size_t i)
{
    return ((ek_val *)bundle.as.ref)[i + 1];
}

static int64_t payload_ns(ek_val v)
{
    return ((ek_val *)v.as.ref)[1].as.i;
}

static ek_val call0(ek_val f)
{
    return ek_closure_call(f, (ek_val *)0, 0);
}

static ek_val call1(ek_val f, ek_val a)
{
    ek_val args[1];

    args[0] = a;
    return ek_closure_call(f, args, 1);
}

static void now_reads_the_wall_clock(void)
{
    ek_val b = ek_cap_clock_wall();
    ek_val t = call0(member(b, 0));
    ek_val t2;
    int64_t ns;

    EK_CHECK(ek_val_is_ref(t));
    EK_CHECK(t.as.ref->info == &ekT_time_Time_Time);
    ns = payload_ns(t);
    /* the wall domain starts around 1677; any real reading is past
       2020 and inside the signed nanosecond span */
    EK_CHECK(ns > (int64_t)1580000000 * 1000000000);
    EK_CHECK(ns < (int64_t)1 << 62);
    ek_release(t);
    t2 = call0(member(b, 0));
    EK_CHECK(payload_ns(t2) >= ns); /* the readings move forward */
    ek_release(t2);
    ek_release(b);
}

static void mono_measures_monotonically(void)
{
    ek_val b = ek_cap_clock_wall();
    ek_val m = call0(member(b, 1));
    ek_val m2;
    int64_t ns;

    EK_CHECK(ek_val_is_ref(m));
    EK_CHECK(m.as.ref->info == &ekT_time_Mono_Mono);
    ns = payload_ns(m);
    ek_release(m);
    m2 = call0(member(b, 1));
    EK_CHECK(payload_ns(m2) >= ns);
    ek_release(m2);
    ek_release(b);
}

static ek_val duration_ns_val(int64_t ns)
{
    ek_obj *o = calloc(1, 16 + sizeof(ek_val));
    ek_val v;

    if (o == NULL) {
        exit(1);
    }
    o->info = &ekT_time_Duration_Duration;
    o->rc = 1;
    ((ek_val *)o)[1] = ek_val_int(ns);
    v = ek_val_ref(o);
    return v;
}

/* A non-positive duration returns immediately; a real one waits. */
static void sleep_waits_and_skips(void)
{
    ek_val b = ek_cap_clock_wall();
    ek_val before = call0(member(b, 1));
    ek_val after;

    call1(member(b, 2), duration_ns_val(0));
    call1(member(b, 2), duration_ns_val(0 - 1000000000));
    after = call0(member(b, 1));
    /* no observable wait for either call */
    EK_CHECK(payload_ns(after) - payload_ns(before) < 500000000);
    ek_release(after);
    ek_release(before);

    before = call0(member(b, 1));
    call1(member(b, 2), duration_ns_val(40000000)); /* 40ms */
    after = call0(member(b, 1));
    EK_CHECK(payload_ns(after) - payload_ns(before) >= 40000000);
    EK_CHECK(payload_ns(after) - payload_ns(before) < 5000000000);
    ek_release(after);
    ek_release(before);
    ek_release(b);
}

/* The bundle holds its three closures and releases them with it. */
static void bundle_structure(void)
{
    ek_val b = ek_cap_clock_wall();
    ek_val now = member(b, 0);
    size_t i;

    EK_CHECK(ek_val_is_ref(b));
    for (i = 1; i <= 3; i++) {
        ek_val m = ((ek_val *)b.as.ref)[i];

        EK_CHECK(ek_val_is_ref(m));
        EK_CHECK_U64(m.as.ref->info->kind, EK_KIND_CLOSURE);
    }
    ek_retain(now);
    EK_CHECK_U64(now.as.ref->rc, 2);
    ek_release(b);
    EK_CHECK_U64(now.as.ref->rc, 1); /* the bundle's drop released one */
    ek_release(now);
}

int main(void)
{
    now_reads_the_wall_clock();
    mono_measures_monotonically();
    sleep_waits_and_skips();
    bundle_structure();
    return ek_test_report("rt/clock");
}
