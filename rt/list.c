#include "internal.h"

#include <stddef.h>
#include <stdlib.h>

/* Lists are immutable singly-linked cons cells ending at the immortal
   empty-list singleton. A cell owns the references it stores: they
   were retained at insertion and are released when the cell dies. The
   spine release walks the chain instead of recursing, so a list of any
   length reclaims in constant C stack. */

typedef struct {
    ek_obj obj;
    ek_val head;
    ek_val tail;
} ek_cons_cell;

/* Nonzero when v refers to a cons cell. */
static int is_cons(ek_val v)
{
    return v.tag == EK_VAL_REF && v.as.ref != NULL
        && v.as.ref->info->kind == EK_KIND_CONS;
}

/* A cell born with one count; the stored references land afterwards. */
static ek_cons_cell *cell_new(void)
{
    ek_cons_cell *cell = ek_rt_alloc_zeroed(sizeof *cell);

    cell->obj.info = &ek_typeinfo_cons;
    cell->obj.rc = 1;
    return cell;
}

ek_val ek_nil_val(void)
{
    return ek_val_ref(ek_nil_obj());
}

ek_val ek_cons(ek_val head, ek_val tail)
{
    ek_cons_cell *cell = cell_new();

    ek_retain(head);
    ek_retain(tail);
    cell->head = head;
    cell->tail = tail;
    return ek_val_ref(&cell->obj);
}

ek_val ek_cons_head(ek_obj *obj)
{
    return ((const ek_cons_cell *)obj)->head;
}

ek_val ek_cons_tail(ek_obj *obj)
{
    return ((const ek_cons_cell *)obj)->tail;
}

ek_val ek_list_append(ek_val a, ek_val b)
{
    ek_val first = ek_nil_val();
    ek_val *slot = &first;
    ek_val cur = a;

    while (is_cons(cur)) {
        const ek_cons_cell *src = (const ek_cons_cell *)cur.as.ref;
        ek_cons_cell *cell = cell_new();
        ek_val made = ek_val_ref(&cell->obj);

        ek_retain(src->head);
        cell->head = src->head;
        *slot = made;
        slot = &cell->tail;
        cur = src->tail;
    }
    ek_retain(b);
    *slot = b;
    return first;
}

size_t ek_list_len(ek_val list)
{
    size_t n = 0;
    ek_val cur = list;

    while (is_cons(cur)) {
        n++;
        cur = ((const ek_cons_cell *)cur.as.ref)->tail;
    }
    return n;
}

/* Releases the head, then consumes the spine: each cell whose count
   reaches zero frees itself here and hands its tail on, and the walk
   stops at a tail that survives elsewhere or at the empty list. */
void ek_cons_drop(ek_obj *obj)
{
    ek_cons_cell *cell = (ek_cons_cell *)obj;
    ek_val cur = cell->tail;

    ek_release(cell->head);
    while (is_cons(cur)) {
        ek_cons_cell *dying = (ek_cons_cell *)cur.as.ref;

        if (ek_rc_dec(&dying->obj) != 0)
            return; /* the tail outlives this cell: its count is done */
        ek_release(dying->head);
        cur = dying->tail;
        free(dying);
    }
    ek_release(cur);
}
