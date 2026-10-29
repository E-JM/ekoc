#include "harness.h"

#include <stdlib.h>

#include "ekort.h"
#include "internal.h"

/* Reference counting: retention, release to zero, cascading child
   release, and the immortal nil singleton. A probe family with two
   child slots and a drop that releases them stands in for the
   compiler-emitted composites. */

#define PROBE_CHILDREN 2
#define DROP_LOG_CAP 16

typedef struct {
    ek_obj obj;
    int id;
    ek_val child[PROBE_CHILDREN];
} probe;

static long drop_count;
static int drop_log[DROP_LOG_CAP];
static int drop_log_len;

static void probe_drop(ek_obj *obj);

static const ek_typeinfo probe_info = { EK_KIND_STRUCT, probe_drop };

/* Releases the children, then records the reclamation order, so the
   log shows children going before their parents. */
static void probe_drop(ek_obj *obj)
{
    probe *p = (probe *)obj;
    int i;

    for (i = 0; i < PROBE_CHILDREN; i++)
        ek_release(p->child[i]);
    if (drop_log_len < DROP_LOG_CAP)
        drop_log[drop_log_len++] = p->id;
    drop_count++;
}

/* Born with one count and no children. */
static probe *probe_new(int id)
{
    probe *p = malloc(sizeof *p);
    int i;

    p->obj.info = &probe_info;
    p->obj.rc = 1;
    p->id = id;
    for (i = 0; i < PROBE_CHILDREN; i++)
        p->child[i] = ek_val_unit();
    return p;
}

static void reset_drop_log(void)
{
    drop_count = 0;
    drop_log_len = 0;
}

static void retain_release_balance(void)
{
    probe *p = probe_new(1);
    ek_val v = ek_val_ref(&p->obj);

    reset_drop_log();
    EK_CHECK_U64(p->obj.rc, 1);
    ek_retain(v);
    EK_CHECK_U64(p->obj.rc, 2);
    ek_retain(v);
    EK_CHECK_U64(p->obj.rc, 3);
    ek_release(v);
    EK_CHECK_U64(p->obj.rc, 2);
    ek_release(v);
    EK_CHECK_U64(p->obj.rc, 1);
    EK_CHECK_U64(drop_count, 0);
    ek_release(v);
    EK_CHECK_U64(drop_count, 1);
}

static void non_reference_tags_are_no_ops(void)
{
    ek_val scalars[5];
    int i;

    reset_drop_log();
    scalars[0] = ek_val_unit();
    scalars[1] = ek_val_int(1);
    scalars[2] = ek_val_uint(2);
    scalars[3] = ek_val_float(3.0);
    scalars[4] = ek_val_char('x');
    for (i = 0; i < 5; i++) {
        ek_retain(scalars[i]);
        ek_release(scalars[i]);
    }
    EK_CHECK_U64(drop_count, 0);
}

static void null_reference_is_no_op(void)
{
    reset_drop_log();
    ek_retain(ek_val_ref(NULL));
    ek_release(ek_val_ref(NULL));
    EK_CHECK_U64(drop_count, 0);
}

/* The parent owns the child's birth reference, so the parent's last
   release reclaims the child first. */
static void child_reclaimed_before_parent(void)
{
    probe *parent = probe_new(10);
    probe *child = probe_new(11);

    reset_drop_log();
    parent->child[0] = ek_val_ref(&child->obj);
    ek_release(ek_val_ref(&parent->obj));
    EK_CHECK_U64(drop_count, 2);
    EK_CHECK_U64(drop_log[0], 11);
    EK_CHECK_U64(drop_log[1], 10);
}

/* A child held elsewhere survives its parent's reclamation, and drops
   only at its own last release. */
static void shared_child_survives_parent(void)
{
    probe *parent = probe_new(20);
    probe *child = probe_new(21);
    ek_val held = ek_val_ref(&child->obj);

    reset_drop_log();
    ek_retain(held);
    parent->child[0] = held;
    ek_release(ek_val_ref(&parent->obj));
    EK_CHECK_U64(drop_count, 1);
    EK_CHECK_U64(child->obj.rc, 1);
    ek_release(held);
    EK_CHECK_U64(drop_count, 2);
    EK_CHECK_U64(drop_log[0], 20);
    EK_CHECK_U64(drop_log[1], 21);
}

static void nil_singleton_is_immortal(void)
{
    ek_obj *nil = ek_nil_obj();
    ek_val v = ek_val_ref(nil);

    reset_drop_log();
    EK_CHECK_U64(nil->info->kind, EK_KIND_NIL);
    EK_CHECK_U64(nil->rc, 1);
    ek_retain(v);
    EK_CHECK_U64(nil->rc, 2);
    ek_release(v);
    EK_CHECK_U64(nil->rc, 1);
    ek_release(v);
    EK_CHECK_U64(nil->rc, 0);
    EK_CHECK_U64(drop_count, 0);
    ek_retain(v);
    EK_CHECK_U64(nil->rc, 1);
    ek_release(v);
    EK_CHECK_U64(nil->rc, 0);
}

int main(void)
{
    retain_release_balance();
    non_reference_tags_are_no_ops();
    null_reference_is_no_op();
    child_reclaimed_before_parent();
    shared_child_survives_parent();
    nil_singleton_is_immortal();
    return ek_test_report("rt/rc");
}
