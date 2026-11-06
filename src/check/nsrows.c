#include "check/internal.h"

/* The injected pairs of the capability module (Clause 10.8): the
   compiler rows behind the `_`-prefixed names the cap source
   references without declaring. Each pair is the unwrap and wrap of
   one session bundle. Their rows carry no declaration node, so the
   schemes hang here and the pair stays unnameable outside the
   capability module. The file bundle is generic over the callback
   result of `with_file`; its variable generalizes, so every use
   instantiates its own. */

const ns_pair_row ck_ns_pair_rows[] = {
    {"_out_unwrap", SIG_FN(EK_ARROW_PURE, SIG_CAP("_OutB"),
                           SIG_STD("Out"))},
    {"_out_wrap", SIG_FN(EK_ARROW_PURE, SIG_STD("Out"),
                         SIG_CAP("_OutB"))},
    {"_in_unwrap", SIG_FN(EK_ARROW_PURE, SIG_CAP("_InB"),
                          SIG_STD("In"))},
    {"_in_wrap", SIG_FN(EK_ARROW_PURE, SIG_STD("In"),
                        SIG_CAP("_InB"))},
    {"_fs_unwrap", SIG_FN(EK_ARROW_PURE, SIG_CAP1("_FsB", SIG_VAR(0)),
                          SIG_STD("Fs"))},
    {"_fs_wrap", SIG_FN(EK_ARROW_PURE, SIG_STD("Fs"),
                        SIG_CAP1("_FsB", SIG_VAR(0)))},
    {"_env_unwrap", SIG_FN(EK_ARROW_PURE, SIG_CAP("_EnvB"),
                           SIG_STD("Env"))},
    {"_env_wrap", SIG_FN(EK_ARROW_PURE, SIG_STD("Env"),
                         SIG_CAP("_EnvB"))},
    {"_args_unwrap", SIG_FN(EK_ARROW_PURE, SIG_CAP("_ArgsB"),
                            SIG_STD("Args"))},
    {"_args_wrap", SIG_FN(EK_ARROW_PURE, SIG_STD("Args"),
                          SIG_CAP("_ArgsB"))},
    {"_clock_unwrap", SIG_FN(EK_ARROW_PURE, SIG_CAP("_ClockB"),
                             SIG_STD("Clock"))},
    {"_clock_wrap", SIG_FN(EK_ARROW_PURE, SIG_STD("Clock"),
                           SIG_CAP("_ClockB"))},
    {"_rand_unwrap", SIG_FN(EK_ARROW_PURE, SIG_CAP("_RandB"),
                            SIG_STD("Rand"))},
    {"_rand_wrap", SIG_FN(EK_ARROW_PURE, SIG_STD("Rand"),
                          SIG_CAP("_RandB"))},
};

/* The pair table's length. */
const size_t ck_ns_pair_rows_len
    = sizeof ck_ns_pair_rows / sizeof ck_ns_pair_rows[0];
