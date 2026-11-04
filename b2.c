#include "ekort.h"

static inline ek_val ek_keep(ek_val v) { ek_retain(v); return v; }

static inline void ek_release_obj(ek_obj *o) { ek_release(ek_val_ref(o)); }

static ek_val ekc0;

typedef struct { ek_obj obj; ek_val f_write; } ekS_cap__OutB;
typedef char ekB_cap__OutB_write[(offsetof(ekS_cap__OutB, f_write) == sizeof(ek_obj) + EK_OUT_WRITE * sizeof(ek_val)) ? 1 : -1];
typedef char ekB_cap__OutB_slots[(sizeof(ekS_cap__OutB) == sizeof(ek_obj) + EK_OUT_SLOTS * sizeof(ek_val)) ? 1 : -1];

static ek_val ekf_b2_main(ek_val ek_p0);
static ek_obj *ekd_show_std_Int(ek_val v);
static ek_val ekf_b2_f(ek_val ek_p0);
static ek_val ekf_cap_Out_println(ek_val ek_p0, ek_val ek_p1);

static ek_val ekf_b2_main(ek_val ek_p0)
{
    ek_val t0 = ek_val_int((int64_t)5ULL);
    ek_val t1 = ekf_b2_f(t0);
    ek_release(t0);
    ek_val t2 = ek_val_ref(ekd_show_std_Int(t1));
    ek_release(t1);
    ek_val t3 = ekf_cap_Out_println(ek_p0, t2);
    ek_release(t2);
    return t3;
}

static ek_val ekf_b2_f(ek_val ek_p0)
{
    ek_val t0 = ek_keep(ek_p0);
    ek_val t1 = ek_val_unit();
    do {
        {
            int t2 = 1;
            t2 = t2 && (t0.as.i == (int64_t)0ULL);
            if (t2) {
                t1 = ek_val_int((int64_t)1ULL);
                break;
            }
        }
        {
            int t3 = 1;
            if (t3) {
                t1 = ek_val_int((int64_t)2ULL);
                break;
            }
        }
        ek_panic("P-panic", "no matching case arm");
    } while (0);
    ek_release(t0);
    return t1;
}

static ek_val ekf_cap_Out_println(ek_val ek_p0, ek_val ek_p1)
{
    ek_val t0 = ek_keep(((ekS_cap__OutB *)ek_p0.as.ref)->f_write);
    ek_val t2 = ek_keep(ek_p1);
    ek_val t3 = ek_keep(ekc0);
    ek_val t4 = ek_val_ref(ek_str_concat(t2.as.ref, t3.as.ref));
    ek_release(t3);
    ek_release(t2);
    ek_val t5[] = {t4};
    ek_val t1 = ek_closure_call(t0, t5, 1);
    ek_release(t4);
    ek_release(t0);
    return t1;
}

static ek_obj *ekd_show_std_Int(ek_val v)
{
    return ek_show_int(v);
}

int main(int argc, char **argv)
{
    ekc0 = ek_val_ref(ek_str_from_cstr("\012"));
    (void)argc; (void)argv;
    ek_val c0 = ek_cap_out_stdout();
    ek_val t = ekf_b2_main(c0);
    ek_release(c0);
    ek_release(t);
    return 0;
}
