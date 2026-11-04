#include "ekort.h"

static inline ek_val ek_keep(ek_val v) { ek_retain(v); return v; }

static inline void ek_release_obj(ek_obj *o) { ek_release(ek_val_ref(o)); }

static ek_val ekc0;

typedef struct { ek_obj obj; ek_val f_write; } ekS_cap__OutB;
typedef char ekB_cap__OutB_write[(offsetof(ekS_cap__OutB, f_write) == sizeof(ek_obj) + EK_OUT_WRITE * sizeof(ek_val)) ? 1 : -1];
typedef char ekB_cap__OutB_slots[(sizeof(ekS_cap__OutB) == sizeof(ek_obj) + EK_OUT_SLOTS * sizeof(ek_val)) ? 1 : -1];

static ek_val ekf_f1_main(ek_val ek_p0);
static ek_obj *ekd_show_std_Float(ek_val v);
static ek_obj *ekd_show_std_Bool(ek_val v);
static ek_val ekf_cap_Out_println(ek_val ek_p0, ek_val ek_p1);

static ek_val ekf_f1_main(ek_val ek_p0)
{
    ek_val big = ek_val_float(1e+308);
    ek_val t0 = ek_keep(big);
    ek_val t1 = ek_val_float(10);
    ek_val t2 = ek_num_mul(t0, t1);
    ek_release(t1);
    ek_release(t0);
    ek_val over = t2;
    ek_val t3 = ek_val_float(1);
    ek_val t4 = ek_val_float(0);
    ek_val t5 = ek_num_div(t3, t4);
    ek_release(t4);
    ek_release(t3);
    ek_val dz = t5;
    ek_val t6 = ek_val_ref(ekd_show_std_Float(over));
    ek_val t7 = ekf_cap_Out_println(ek_p0, t6);
    ek_release(t6);
    ek_release(t7);
    ek_val t8 = ek_keep(dz);
    ek_val t9 = ek_val_float(0);
    ek_val t10 = ek_num_gt(t8, t9);
    ek_release(t9);
    ek_release(t8);
    ek_val t11 = ek_val_ref(ekd_show_std_Bool(t10));
    ek_release(t10);
    ek_val t12 = ekf_cap_Out_println(ek_p0, t11);
    ek_release(t11);
    ek_release(dz);
    ek_release(over);
    ek_release(big);
    return t12;
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

static ek_obj *ekd_show_std_Float(ek_val v)
{
    return ek_show_float(v);
}

static ek_obj *ekd_show_std_Bool(ek_val v)
{
    return ek_show_bool(v);
}

int main(int argc, char **argv)
{
    ekc0 = ek_val_ref(ek_str_from_cstr("\012"));
    (void)argc; (void)argv;
    ek_val c0 = ek_cap_out_stdout();
    ek_val t = ekf_f1_main(c0);
    ek_release(c0);
    ek_release(t);
    return 0;
}
