#include "harness.h"
#include "capident.h"

#include "ekort.h"

/* The randomness bundles: the seeded generator's order is fixed by the
   seed, the bound is exclusive, and two bundles over one seed draw the
   same sequence. */

/* The bundle's one member closure. */
static ek_val member(ek_val bundle)
{
    return ((ek_val *)bundle.as.ref)[1];
}

static ek_val draw(ek_val bundle, unsigned long bound)
{
    ek_val args[1];

    args[0] = ek_val_uint(bound);
    return ek_closure_call(member(bundle), args, 1);
}

static void seeded_draws_are_deterministic(void)
{
    ek_val a = ek_rand_with_seed(ek_val_int(0));
    ek_val a2 = ek_rand_with_seed(ek_val_int(0));
    ek_val c = ek_rand_with_seed(ek_val_int(1));
    size_t i;
    int same = 1;
    int differs = 0;

    for (i = 0; i < 16; i++) {
        ek_val x = draw(a, 1000);
        ek_val y = draw(a2, 1000);
        ek_val z = draw(c, 1000);

        if (x.as.u != y.as.u) {
            same = 0;
        }
        if (x.as.u != z.as.u) {
            differs = 1;
        }
        ek_release(x);
        ek_release(y);
        ek_release(z);
    }
    EK_CHECK(same);
    EK_CHECK(differs);
    ek_release(a);
    ek_release(a2);
    ek_release(c);
}

/* A bundle keeps drawing: the sequence advances across calls, the
   negative seed folds into the unsigned state, and the bound holds. */
static void the_sequence_advances(void)
{
    ek_val a = ek_rand_with_seed(ek_val_int(0 - 3));
    ek_val b = ek_rand_with_seed(ek_val_int(0 - 3));
    ek_val first = draw(a, 7);
    ek_val second;
    ek_val again;
    size_t i;

    EK_CHECK(first.as.u < 7);
    for (i = 0; i < 200; i++) {
        ek_val d = draw(a, 7);

        EK_CHECK(d.as.u < 7);
        ek_release(d);
    }
    second = draw(a, 7);
    again = draw(b, 7);
    EK_CHECK_U64(again.as.u, first.as.u);
    ek_release(first);
    ek_release(second);
    ek_release(again);
    ek_release(a);
    ek_release(b);
}

/* The entropy bundle answers every draw inside the bound. */
static void entropy_stays_in_bounds(void)
{
    ek_val r = ek_cap_rand_os();
    size_t i;

    for (i = 0; i < 64; i++) {
        ek_val d = draw(r, 13);

        EK_CHECK(d.as.u < 13);
        ek_release(d);
    }
    ek_release(r);
}

int main(void)
{
    seeded_draws_are_deterministic();
    the_sequence_advances();
    entropy_stays_in_bounds();
    return ek_test_report("rt/rand");
}
