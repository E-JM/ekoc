#include "internal.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* Sets are immutable collections of distinct elements with
   copy-on-insert: inserting builds a fresh table, and the old set keeps
   its references. The table is the dictionary's discipline -- open
   addressing with linear probing that grows from eight slots and stays
   at or under seventy percent full, with the build order kept in a
   chain of entry indices, so iteration walks insertion order even
   after the elements rehash into new slots.

   Each set carries the eq and hash hooks its element domain was built
   with, and every derived construction clones the source's pair. The
   domains the runtime itself can key -- the scalars, the Float rules,
   and the String and Bytes families -- answer before any hook runs; a
   reference element outside them is answered by the carried pair, so
   two structurally equal composite elements are one. An element the
   carried pair cannot answer aborts the process. */

#define SET_LOAD_NUM 10
#define SET_LOAD_DEN 7
#define SET_MIN_CAP 8

/* The chain index that marks the end of the build order. */
#define SET_NO_ENTRY UINT32_MAX

typedef struct {
    ek_val key;
    uint32_t next;
    uint8_t used;
} ek_set_entry;

typedef struct {
    ek_obj obj;
    size_t count;
    size_t capacity;
    uint32_t head;
    uint32_t tail;
    ek_key_eq_fn keq;
    ek_key_hash_fn kh;
    ek_set_entry *entries;
} ek_set;

/* The payload bytes that identify a scalar: a Char's code is four
   bytes wide, and every other payload fills the whole slot. */
static size_t payload_width(const ek_val *v)
{
    return v->tag == EK_VAL_CHAR ? sizeof v->as.c : sizeof v->as;
}

/* A String or Bytes value keys its bytes, not its address, so two
   spellings of one sequence read as one element. */
static int content_key(const ek_val *v)
{
    if (v->tag != EK_VAL_REF || v->as.ref == NULL)
        return 0;
    return v->as.ref->info->kind == EK_KIND_STR
        || v->as.ref->info->kind == EK_KIND_BYTES;
}

/* The identifying bytes of a String or Bytes element. */
static void content_view(const ek_val *v, const unsigned char **p,
                         size_t *width)
{
    if (v->as.ref->info->kind == EK_KIND_STR) {
        *p = (const unsigned char *)ek_str_cstr(v->as.ref);
        *width = ek_str_len(v->as.ref);
    } else {
        *p = ek_bytes_data(v->as.ref);
        *width = ek_bytes_len(v->as.ref);
    }
}

/* The slot word of an element: the carried hash hook answers the
   domains the runtime cannot key itself. A missing hook is an element
   domain the table cannot key, not a lookup miss. */
static unsigned long hash_key(const ek_set *set, ek_val v)
{
    if (content_key(&v))
        return (unsigned long)ek_key_hash(v);
    if (v.tag == EK_VAL_REF && set->kh)
        return set->kh(v);
    if (v.tag != EK_VAL_REF)
        return (unsigned long)ek_key_hash(v);
    ek_abort("the set element carries no hash hook");
    return 0;
}

/* Nonzero when the two elements are the same element. The fast
   domains answer first; a reference element outside them goes to the
   carried eq hook. A missing hook is an element domain the table
   cannot key, not a lookup miss. */
static int key_eq(const ek_set *set, ek_val a, ek_val b)
{
    if (content_key(&a) || content_key(&b)) {
        const unsigned char *pa, *pb;
        size_t wa, wb;

        if (!content_key(&a) || !content_key(&b))
            return 0;
        if (a.as.ref->info->kind != b.as.ref->info->kind)
            return 0;
        content_view(&a, &pa, &wa);
        content_view(&b, &pb, &wb);
        return wa == wb && memcmp(pa, pb, wa) == 0;
    }
    if (a.tag != b.tag)
        return 0;
    if (a.tag == EK_VAL_FLOAT)
        return a.as.f == b.as.f;
    if (a.tag == EK_VAL_REF) {
        if (a.as.ref == NULL || b.as.ref == NULL)
            return a.as.ref == b.as.ref;
        if (set->keq)
            return set->keq(a, b);
        ek_abort("the set element carries no equality hook");
    }
    return memcmp(&a.as, &b.as, payload_width(&a)) == 0;
}

/* The index of the slot the element occupies, or of the free slot
   where it belongs. */
static size_t probe(const ek_set *set, ek_val key)
{
    size_t mask = set->capacity - 1;
    size_t i = hash_key(set, key) & mask;

    while (set->entries[i].used && !key_eq(set, set->entries[i].key, key))
        i = (i + 1) & mask;
    return i;
}

/* The entry storing the element, or NULL when it is absent. */
static const ek_set_entry *find_entry(const ek_set *set, ek_val key)
{
    size_t i;

    if (set->count == 0)
        return NULL;
    i = probe(set, key);
    return set->entries[i].used ? &set->entries[i] : NULL;
}

/* A set object with no table: the empty set over the given element
   hooks. The chain of none starts and ends at the no-entry mark. */
static ek_set *set_new(ek_key_eq_fn keq, ek_key_hash_fn kh)
{
    ek_set *set = ek_rt_alloc_zeroed(sizeof *set);

    set->obj.info = &ek_typeinfo_set;
    set->obj.rc = 1;
    set->keq = keq;
    set->kh = kh;
    set->head = SET_NO_ENTRY;
    set->tail = SET_NO_ENTRY;
    return set;
}

/* A fresh set whose table holds count entries below the load limit,
   carrying the given element hooks: the capacity starts at eight and
   doubles until it fits. */
static ek_set *set_sized_for(size_t count, ek_key_eq_fn keq,
                             ek_key_hash_fn kh)
{
    ek_set *set = set_new(keq, kh);
    size_t capacity = SET_MIN_CAP;

    while (count * SET_LOAD_NUM > capacity * SET_LOAD_DEN)
        capacity *= 2;
    set->capacity = capacity;
    set->entries = ek_rt_alloc_zeroed(capacity * sizeof *set->entries);
    return set;
}

/* Retains the element, stores it in the slot it belongs to, and
   appends the slot to the build-order chain. */
static void entry_store(ek_set *set, ek_val key)
{
    size_t i = probe(set, key);
    ek_set_entry *slot = &set->entries[i];

    ek_retain(key);
    slot->key = key;
    slot->next = SET_NO_ENTRY;
    slot->used = 1;
    if (set->tail == SET_NO_ENTRY)
        set->head = (uint32_t)i;
    else
        set->entries[set->tail].next = (uint32_t)i;
    set->tail = (uint32_t)i;
    set->count++;
}

ek_val ek_set_new(ek_key_eq_fn keq, ek_key_hash_fn kh)
{
    return ek_val_ref(&set_new(keq, kh)->obj);
}

ek_val ek_set_of(ek_val list, ek_key_eq_fn keq, ek_key_hash_fn kh)
{
    ek_set *set = set_sized_for(ek_list_len(list), keq, kh);
    ek_val cur;

    for (cur = list; ek_val_is_cons(cur); cur = ek_cons_tail(cur.as.ref)) {
        ek_val elem = ek_cons_head(cur.as.ref);

        if (find_entry(set, elem) == NULL)
            entry_store(set, elem);
    }
    return ek_val_ref(&set->obj);
}

ek_val ek_set_insert(ek_val s, ek_val elem)
{
    const ek_set *old = (const ek_set *)s.as.ref;
    ek_set *fresh;
    uint32_t idx;

    if (find_entry(old, elem) != NULL) {
        ek_retain(s);
        return s;
    }
    fresh = set_sized_for(old->count + 1, old->keq, old->kh);
    for (idx = old->head; idx != SET_NO_ENTRY; idx = old->entries[idx].next)
        entry_store(fresh, old->entries[idx].key);
    entry_store(fresh, elem);
    return ek_val_ref(&fresh->obj);
}

ek_val ek_set_remove(ek_val s, ek_val elem)
{
    const ek_set *old = (const ek_set *)s.as.ref;
    ek_set *fresh;
    uint32_t idx;

    if (find_entry(old, elem) == NULL) {
        ek_retain(s);
        return s;
    }
    fresh = set_sized_for(old->count - 1, old->keq, old->kh);
    for (idx = old->head; idx != SET_NO_ENTRY; idx = old->entries[idx].next) {
        if (!key_eq(old, old->entries[idx].key, elem))
            entry_store(fresh, old->entries[idx].key);
    }
    return ek_val_ref(&fresh->obj);
}

int ek_set_contains(ek_val s, ek_val elem)
{
    return find_entry((const ek_set *)s.as.ref, elem) != NULL;
}

/* The union keeps the left order first and appends the right set's new
   elements in right order, over the left set's element hooks. */
ek_val ek_set_union(ek_val a, ek_val b)
{
    const ek_set *left = (const ek_set *)a.as.ref;
    const ek_set *right = (const ek_set *)b.as.ref;
    ek_set *fresh = set_sized_for(left->count + right->count, left->keq,
                                  left->kh);
    uint32_t idx;

    for (idx = left->head; idx != SET_NO_ENTRY; idx = left->entries[idx].next)
        entry_store(fresh, left->entries[idx].key);
    for (idx = right->head; idx != SET_NO_ENTRY;
         idx = right->entries[idx].next) {
        ek_val elem = right->entries[idx].key;

        if (find_entry(fresh, elem) == NULL)
            entry_store(fresh, elem);
    }
    return ek_val_ref(&fresh->obj);
}

/* The intersection keeps the left order, keeping the shared elements;
   the difference keeps the left order, dropping them. */
static ek_val set_keep_if(ek_val a, ek_val b, int shared)
{
    const ek_set *left = (const ek_set *)a.as.ref;
    const ek_set *right = (const ek_set *)b.as.ref;
    ek_set *fresh = set_sized_for(left->count, left->keq, left->kh);
    uint32_t idx;

    for (idx = left->head; idx != SET_NO_ENTRY; idx = left->entries[idx].next) {
        ek_val elem = left->entries[idx].key;

        if ((find_entry(right, elem) != NULL) == shared)
            entry_store(fresh, elem);
    }
    return ek_val_ref(&fresh->obj);
}

ek_val ek_set_intersect(ek_val a, ek_val b)
{
    return set_keep_if(a, b, 1);
}

ek_val ek_set_diff(ek_val a, ek_val b)
{
    return set_keep_if(a, b, 0);
}

size_t ek_set_len(ek_val s)
{
    return ((const ek_set *)s.as.ref)->count;
}

ek_val ek_set_to_list(ek_val s)
{
    const ek_set *set = (const ek_set *)s.as.ref;
    ek_elems elems;
    uint32_t idx;

    elems.data = NULL;
    elems.len = 0;
    elems.cap = 0;
    for (idx = set->head; idx != SET_NO_ENTRY; idx = set->entries[idx].next) {
        ek_val v = set->entries[idx].key;

        ek_retain(v);
        ek_elems_add(&elems, v);
    }
    return ek_elems_list(&elems);
}

/* Releases every used entry's element, walking the table in order;
   the release path frees the set object itself. */
void ek_set_drop(ek_obj *obj)
{
    ek_set *set = (ek_set *)obj;
    size_t i;

    for (i = 0; i < set->capacity; i++) {
        if (set->entries[i].used)
            ek_release(set->entries[i].key);
    }
    free(set->entries);
}
