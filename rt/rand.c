#define _POSIX_C_SOURCE 200809L

#include "internal.h"

#include <stdint.h>
#include <stdio.h>

/* The randomness bundles. The generator is splitmix64: the seed fixes
   the order of the draws, the sequence itself is an implementation
   detail, and the bound is exclusive. A zero bound is the invariant
   violation the panic table assigns to P-panic. The entropy bundle
   seeds the generator from the operating system. */

typedef struct {
    ek_obj obj;
    uint64_t state;
} ek_rand_cell;

static const ek_typeinfo ek_typeinfo_rand_cell = { EK_KIND_STRUCT, NULL, NULL, NULL };

static uint64_t splitmix64(uint64_t *state)
{
    uint64_t z = (*state += 0x9E3779B97F4A7C15ULL);

    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static ek_val rand_int(ek_val env, ek_val *args, size_t argc)
{
    ek_rand_cell *cell;

    if (argc != 1 || args[0].tag != EK_VAL_UINT || !ek_val_is_ref(env)
        || env.as.ref->info != &ek_typeinfo_rand_cell) {
        return ek_val_uint(0);
    }
    cell = (ek_rand_cell *)env.as.ref;
    if (args[0].as.u == 0) {
        ek_panic("P-panic", "Rand.int: the bound is zero");
    }
    return ek_val_uint(splitmix64(&cell->state) % args[0].as.u);
}

/* A Rand bundle over the generator state: the state moves into the
   closure's environment. */
static ek_val rand_bundle(uint64_t seed)
{
    ek_rand_cell *cell = ek_rt_alloc_zeroed(sizeof *cell);
    ek_val env;
    ek_val bundle;

    cell->obj.info = &ek_typeinfo_rand_cell;
    cell->obj.rc = 1;
    cell->state = seed;
    env = ek_val_ref(&cell->obj);
    bundle = ek_bundle1_new(ek_closure_new(rand_int, env));
    ek_release(env);
    return bundle;
}

ek_val ek_rand_with_seed(ek_val seed)
{
    uint64_t s = 0;

    if (seed.tag == EK_VAL_INT) {
        s = (uint64_t)seed.as.i;
    }
    return rand_bundle(s);
}

/* The operating system's entropy, best effort: the first eight bytes
   of the entropy source as the seed, a fixed fallback when they cannot
   be read. */
ek_val ek_cap_rand_os(void)
{
    uint64_t seed = 0x5DEECE66DULL;
    FILE *fp = fopen("/dev/urandom", "rb");

    if (fp != NULL) {
        if (fread(&seed, 1, sizeof seed, fp) != sizeof seed) {
            seed = 0x5DEECE66DULL;
        }
        fclose(fp);
    }
    return rand_bundle(seed);
}
