#include "internal.h"

#include <stddef.h>

/* Vectors are immutable, persistent sequences with random access by
   position: a balanced binary tree of the elements, one element per
   leaf, every interior node carrying its subtree's size and height.
   An update copies the path it descends and shares every subtree it
   does not touch, so get, set, and push all stay within logarithmic
   new nodes and the old version keeps its elements. */

typedef struct ek_vec {
    ek_obj obj;
    struct ek_vec *l;   /* both NULL at a leaf */
    struct ek_vec *r;
    size_t size;        /* the subtree's element count */
    int h;              /* the subtree's height */
    ek_val elem;        /* the leaf's element; unit elsewhere */
} ek_vec;

static void vec_drop(ek_obj *o);

const ek_typeinfo ek_typeinfo_vector = { EK_KIND_VECTOR, vec_drop, NULL,
                                         NULL };

static ek_val node_val(ek_vec *v)
{
    return ek_val_ref(&v->obj);
}

/* A fresh node over no elements: the empty vector, a leaf with no
   element of its own. */
static ek_vec *vec_empty(void)
{
    ek_vec *v = ek_rt_alloc_zeroed(sizeof *v);

    v->obj.info = &ek_typeinfo_vector;
    v->obj.rc = 1;
    v->elem = ek_val_unit();
    return v;
}

static void vec_drop(ek_obj *o)
{
    ek_vec *v = (ek_vec *)o;

    if (v->l) {
        ek_release(node_val(v->l));
        ek_release(node_val(v->r));
    } else if (v->size) {
        ek_release(v->elem);
    }
}

/* A fresh leaf over one retained element. */
static ek_vec *vec_leaf(ek_val e)
{
    ek_vec *v = vec_empty();

    ek_retain(e);
    v->elem = e;
    v->size = 1;
    v->h = 1;
    return v;
}

/* A fresh branch over two retained children. */
static ek_vec *vec_branch(ek_vec *l, ek_vec *r)
{
    ek_vec *v = vec_empty();

    ek_retain(node_val(l));
    ek_retain(node_val(r));
    v->l = l;
    v->r = r;
    v->size = l->size + r->size;
    v->h = (l->h > r->h ? l->h : r->h) + 1;
    return v;
}

/* One level over children whose heights differ by at most two, the
   taller side rotating down to keep every level within one. */
static ek_vec *vec_join(ek_vec *l, ek_vec *r)
{
    if (l->h - r->h > 1) {
        if (l->l->h >= l->r->h)
            return vec_branch(l->l, vec_branch(l->r, r));
        return vec_branch(vec_branch(l->l, l->r->l),
                          vec_branch(l->r->r, r));
    }
    if (r->h - l->h > 1) {
        if (r->r->h >= r->l->h)
            return vec_branch(vec_branch(l, r->l), r->r);
        return vec_branch(vec_branch(l, r->l->l),
                          vec_branch(r->l->r, r->r));
    }
    return vec_branch(l, r);
}

/* The leaf at position i, with 0 <= i < size. */
static const ek_vec *vec_at(const ek_vec *v, size_t i)
{
    while (v->l) {
        if (i < v->l->size) {
            v = v->l;
        } else {
            i -= v->l->size;
            v = v->r;
        }
    }
    return v;
}

/* The tree of the first n elements of the cells at *cursor, in their
   order, balanced by construction; the cursor ends past them. */
static ek_vec *vec_build(ek_val *cursor, size_t n)
{
    if (n == 0) {
        return vec_empty();
    }
    if (n == 1) {
        ek_obj *cell = cursor->as.ref;
        ek_vec *leaf = vec_leaf(ek_cons_head(cell));

        *cursor = ek_cons_tail(cell);
        return leaf;
    }
    {
        ek_vec *l = vec_build(cursor, n / 2);
        ek_vec *r = vec_build(cursor, n - n / 2);

        return vec_branch(l, r);
    }
}

/* The tree with e in place of the element at i, the path copied and
   the shape untouched. */
static ek_vec *vec_update(ek_vec *v, size_t i, ek_val e)
{
    if (!v->l) {
        return vec_leaf(e);
    }
    if (i < v->l->size) {
        return vec_branch(vec_update(v->l, i, e), v->r);
    }
    return vec_branch(v->l, vec_update(v->r, i - v->l->size, e));
}

/* The tree with e inserted at i, 0 <= i <= size, the path copied and
   rebalanced. */
static ek_vec *vec_insert(ek_vec *v, size_t i, ek_val e)
{
    if (v->size == 0) {
        return vec_leaf(e);
    }
    if (!v->l) {
        return i == 0 ? vec_branch(vec_leaf(e), v) : vec_branch(v,
                                                               vec_leaf(e));
    }
    if (i <= v->l->size) {
        return vec_join(vec_insert(v->l, i, e), v->r);
    }
    return vec_join(v->l, vec_insert(v->r, i - v->l->size, e));
}

/* The elements of one subtree, in order, onto the accumulator. */
static void vec_collect(const ek_vec *v, ek_elems *es)
{
    if (!v->l) {
        if (v->size) {
            ek_retain(v->elem);
            ek_elems_add(es, v->elem);
        }
        return;
    }
    vec_collect(v->l, es);
    vec_collect(v->r, es);
}

/* Reads the index word; nonzero when it names a position inside the
   vector. */
static int vec_index(ek_val v, const ek_vec *vec, size_t *out)
{
    if (v.tag != EK_VAL_INT || v.as.i < 0
        || (size_t)v.as.i >= vec->size) {
        return 0;
    }
    *out = (size_t)v.as.i;
    return 1;
}

ek_val ek_vec_new(void)
{
    return node_val(vec_empty());
}

ek_val ek_vec_of(ek_val list)
{
    ek_val cursor = list;
    ek_vec *v = vec_build(&cursor, ek_list_len(list));

    return node_val(v);
}

ek_val ek_vec_to_list(ek_val v)
{
    ek_elems es;

    es.data = NULL;
    es.len = 0;
    es.cap = 0;
    vec_collect((const ek_vec *)v.as.ref, &es);
    return ek_elems_list(&es);
}

size_t ek_vec_len(ek_val v)
{
    return ((const ek_vec *)v.as.ref)->size;
}

int ek_vec_get(ek_val v, ek_val index, ek_val *out)
{
    const ek_vec *vec = (const ek_vec *)v.as.ref;
    size_t i;

    if (!vec_index(index, vec, &i)) {
        return 0;
    }
    *out = vec_at(vec, i)->elem;
    return 1;
}

ek_obj *ek_vec_set(ek_val v, ek_val index, ek_val elem)
{
    ek_vec *vec = (ek_vec *)v.as.ref;
    size_t i;

    if (!vec_index(index, vec, &i)) {
        return NULL;
    }
    return &vec_update(vec, i, elem)->obj;
}

ek_val ek_vec_push(ek_val v, ek_val elem)
{
    ek_vec *vec = (ek_vec *)v.as.ref;

    return node_val(vec_insert(vec, vec->size, elem));
}

/* The in-order walk machine behind a dictionary's order vector. begin
   retains the vector and stacks the root; each step pops a node, and
   a branch spreads its children right over left so the left subtree
   drains first, so the walk yields every position in order at one
   step per element. */

void ek_vec_iter_begin(ek_vec_iter *it, ek_val v)
{
    it->hold = v;
    ek_retain(v);
    it->top = 0;
    it->index = 0;
    if (((const ek_vec *)v.as.ref)->size) {
        it->path[it->top++] = v.as.ref;
    }
}

int ek_vec_iter_next(ek_vec_iter *it, size_t *index, ek_val *elem)
{
    const ek_vec *n;

    while (it->top > 0) {
        n = it->path[--it->top];
        if (!n->l) {
            *index = it->index++;
            *elem = n->elem;
            return 1;
        }
        it->path[it->top++] = n->r;
        it->path[it->top++] = n->l;
    }
    return 0;
}

void ek_vec_iter_end(ek_vec_iter *it)
{
    ek_release(it->hold);
    it->hold = ek_val_unit();
    it->top = 0;
}
