#include "ekort.h"

#include <stdlib.h>

/* Counts are always atomic: a value shared between tasks must not race
   on its count, and correctness comes before optimization. C99 has no
   _Atomic, so the compiler's atomic builtins carry every count; the
   legacy __sync builtins cover compilers without them. */
#if defined(__has_builtin)
#  if __has_builtin(__atomic_fetch_add)
#    define RT_HAS_ATOMIC_BUILTIN 1
#  endif
#endif

#ifdef RT_HAS_ATOMIC_BUILTIN
static unsigned long count_inc(unsigned long *count)
{
    return __atomic_fetch_add(count, 1, __ATOMIC_SEQ_CST);
}

static unsigned long count_dec(unsigned long *count)
{
    return __atomic_fetch_sub(count, 1, __ATOMIC_SEQ_CST);
}
#else
static unsigned long count_inc(unsigned long *count)
{
    return __sync_fetch_and_add(count, 1);
}

static unsigned long count_dec(unsigned long *count)
{
    return __sync_fetch_and_sub(count, 1);
}
#endif

/* The count primitive of the iterative release cascade; the contract
   with the families is in internal.h. */
unsigned long ek_rc_dec(ek_obj *obj)
{
    return count_dec(&obj->rc) - 1;
}

void ek_retain(ek_val v)
{
    if (v.tag != EK_VAL_REF || v.as.ref == NULL)
        return;
    count_inc(&v.as.ref->rc);
}

void ek_release(ek_val v)
{
    ek_obj *obj;

    if (v.tag != EK_VAL_REF || v.as.ref == NULL)
        return;
    obj = v.as.ref;
    if (count_dec(&obj->rc) != 1)
        return;
    if (obj->info->kind == EK_KIND_NIL)
        return; /* the immortal singleton: static storage, no drop */
    if (obj->info->drop != NULL)
        obj->info->drop(obj);
    free(obj);
}
