#define _POSIX_C_SOURCE 200809L

#include "internal.h"

#include <errno.h>
#include <time.h>

/* The wall-clock bundle: the readings come from the platform's
   realtime and monotonic clocks and the sleep waits through
   nanosleep, resuming after an interrupt. A non-positive duration
   returns immediately. The readings are the time companion's values,
   built over the emitted unit's identities with the nanosecond
   payload; the arithmetic on them stays in Eko source. */

#define NS_PER_S 1000000000

static int64_t now_ns(clockid_t clock)
{
    struct timespec ts;

    if (clock_gettime(clock, &ts) != 0) {
        return 0;
    }
    return (int64_t)ts.tv_sec * NS_PER_S + (int64_t)ts.tv_nsec;
}

static ek_val clock_now(ek_val env, ek_val *args, size_t argc)
{
    (void)env;
    (void)args;
    if (argc != 0) {
        return ek_gen_time(0);
    }
    return ek_gen_time(now_ns(CLOCK_REALTIME));
}

static ek_val clock_mono(ek_val env, ek_val *args, size_t argc)
{
    (void)env;
    (void)args;
    if (argc != 0) {
        return ek_gen_mono(0);
    }
    return ek_gen_mono(now_ns(CLOCK_MONOTONIC));
}

/* The duration argument's nanosecond payload, read through the layout
   contract for the time companion's constructor families. */
static ek_val clock_sleep(ek_val env, ek_val *args, size_t argc)
{
    struct timespec rest;
    int64_t ns;

    (void)env;
    if (argc != 1) {
        return ek_val_unit();
    }
    ns = ek_gen_ns(args[0]);
    if (ns <= 0) {
        return ek_val_unit();
    }
    rest.tv_sec = (time_t)(ns / NS_PER_S);
    rest.tv_nsec = (long)(ns % NS_PER_S);
    while (nanosleep(&rest, &rest) != 0 && errno == EINTR) {
    }
    return ek_val_unit();
}

ek_val ek_cap_clock_wall(void)
{
    ek_val slots[EK_CLOCK_SLOTS];

    slots[EK_CLOCK_NOW] = ek_closure_new(clock_now, ek_val_unit());
    slots[EK_CLOCK_MONO] = ek_closure_new(clock_mono, ek_val_unit());
    slots[EK_CLOCK_SLEEP] = ek_closure_new(clock_sleep, ek_val_unit());
    return ek_bundle3_new(slots);
}
