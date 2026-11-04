#define _POSIX_C_SOURCE 200809L

#include "harness.h"
#include "capident.h"

#include <stdlib.h>
#include <string.h>

#include "ekort.h"

/* The process surfaces: the environment over the real variables, the
   argument vector over a constructed array, and the process token.
   The Option payloads read through the layout contract; the Some and
   None identities are the ones the unit defines. */

/* The bundle's one member closure. */
static ek_val member(ek_val bundle)
{
    return ((ek_val *)bundle.as.ref)[1];
}

/* The payload of a Some, borrowed. */
static ek_val some_payload(ek_val v)
{
    return ((ek_val *)v.as.ref)[1];
}

static int is_some(ek_val v)
{
    return v.as.ref->info == &ekT_std$Option$Some;
}

static void env_reads_the_variables(void)
{
    ek_val b;
    ek_val args[1];
    ek_val r;

    EK_CHECK(setenv("EKO_RT_ENV_PROBE", "value 1", 1) == 0);
    EK_CHECK(unsetenv("EKO_RT_ENV_ABSENT") == 0);
    b = ek_cap_env_os();
    EK_CHECK(ek_val_is_ref(b));
    args[0] = ek_val_ref(ek_str_from_cstr("EKO_RT_ENV_PROBE"));
    r = ek_closure_call(member(b), args, 1);
    EK_CHECK(is_some(r));
    if (is_some(r)) {
        ek_val v = some_payload(r);

        EK_CHECK_STR(ek_str_cstr(v.as.ref), "value 1");
        ek_release(v);
    }
    ek_release(r);
    args[0].as.ref = ek_str_from_cstr("EKO_RT_ENV_ABSENT");
    r = ek_closure_call(member(b), args, 1);
    EK_CHECK(ek_val_is_ref(r));
    EK_CHECK(r.as.ref->info == &ekT_std$Option$None);
    ek_release(r);
    ek_release(ek_val_ref(args[0].as.ref));
    ek_release(b);
}

static void args_carry_the_vector(void)
{
    char *av[3];
    ek_val b;
    ek_val all;
    ek_val it;

    av[0] = "one";
    av[1] = "two";
    b = ek_cap_args(2, av);
    EK_CHECK(ek_val_is_ref(b));
    all = ek_closure_call(member(b), (ek_val *)0, 0);
    EK_CHECK_U64(ek_list_len(all), 2); /* the program name is absent */
    it = all;
    EK_CHECK(ek_val_is_cons(it));
    EK_CHECK_STR(ek_str_cstr(ek_cons_head(it.as.ref).as.ref), "one");
    it = ek_cons_tail(it.as.ref);
    EK_CHECK_STR(ek_str_cstr(ek_cons_head(it.as.ref).as.ref), "two");
    ek_release(all);

    /* each call builds a fresh list: the first survives this one */
    all = ek_closure_call(member(b), (ek_val *)0, 0);
    EK_CHECK_U64(ek_list_len(all), 2);
    ek_release(all);

    /* the empty vector is the empty list */
    ek_release(b);
    b = ek_cap_args(0, (char **)0);
    all = ek_closure_call(member(b), (ek_val *)0, 0);
    EK_CHECK(ek_val_is_nil(all));
    ek_release(all);
    ek_release(b);
}

static void proc_token_is_opaque(void)
{
    ek_val p = ek_cap_proc();

    EK_CHECK(ek_val_is_ref(p));
    EK_CHECK_U64(p.as.ref->info->kind, EK_KIND_STRUCT);
    EK_CHECK_U64(p.as.ref->rc, 1);
    ek_retain(p);
    EK_CHECK_U64(p.as.ref->rc, 2);
    ek_release(p);
    EK_CHECK_U64(p.as.ref->rc, 1);
    ek_release(p);
}

int main(void)
{
    env_reads_the_variables();
    args_carry_the_vector();
    proc_token_is_opaque();
    return ek_test_report("rt/capenv");
}
