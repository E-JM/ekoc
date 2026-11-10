#include "internal.h"

/* The std Result and Option constructor wrappers the capability
   closures and the document reader hand back. Every wrapper stamps
   the emitted unit's typeinfo, so generated pattern matches and drops
   treat the value as one of their own. The generic builders behind
   them live in genword.c, an object that names no family's typeinfo. */

ek_val ek_gen_result_ok(ek_val payload)
{
    return ek_gen_one(&ekT_std$Result$Ok, payload);
}

ek_val ek_gen_result_err(ek_val payload)
{
    return ek_gen_one(&ekT_std$Result$Err, payload);
}

ek_val ek_gen_option_some(ek_val payload)
{
    return ek_gen_one(&ekT_std$Option$Some, payload);
}

ek_val ek_gen_option_none(void)
{
    return ek_gen_zero(&ekT_std$Option$None);
}
