#include "internal.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* Dictionaries are immutable maps with copy-on-insert: setting a key
   builds a fresh table, and the old dictionary keeps its references.
   Keys are inline scalars, hashed with FNV-1a over the tag byte and
   the payload bytes; the table is open addressing with linear probing
   that grows from eight slots and stays at or under seventy percent
   full, so a probe always finds an empty slot. The build order of
   each table is kept in a chain of entry indices, so iteration walks
   insertion order even after the keys rehash into new slots. */

/* The load limit and the starting capacity of the table. */
#define DICT_LOAD_NUM 10
#define DICT_LOAD_DEN 7
#define DICT_MIN_CAP 8

/* The chain index that marks the end of the build order. */
#define DICT_NO_ENTRY UINT32_MAX

typedef struct {
    ek_val key;
    ek_val val;
    uint32_t next;
    uint8_t used;
} ek_dict_entry;

typedef struct {
    ek_obj obj;
    size_t count;
    size_t capacity;
    uint32_t head;
    uint32_t tail;
    ek_dict_entry *entries;
} ek_dict;

/* The payload bytes that identify a scalar: a Char's code is four
   bytes wide, and every other payload fills the whole slot. The rest
   of the slot is not part of the value. */
static size_t payload_width(const ek_val *v)
{
    return v->tag == EK_VAL_CHAR ? sizeof v->as.c : sizeof v->as;
}

/* A String value keys its bytes, not its address, so two spellings of
   one text read as one key: the key domain compares structurally. */
static int text_key(const ek_val *v)
{
    return v->tag == EK_VAL_REF && v->as.ref != NULL
        && v->as.ref->info->kind == EK_KIND_STR;
}

/* FNV-1a over the tag byte and the identifying bytes, so keys of
   different tags stay distinct even when their payloads coincide. */
static uint64_t hash_key(ek_val v)
{
    uint64_t h = 14695981039346656037ULL;
    const unsigned char *p = (const unsigned char *)&v.as;
    size_t width;

    if (text_key(&v)) {
        p = (const unsigned char *)ek_str_cstr(v.as.ref);
        width = ek_str_len(v.as.ref);
    } else {
        width = payload_width(&v);
    }
    h = (h ^ (uint64_t)v.tag) * 1099511628211ULL;
    for (size_t i = 0; i < width; i++)
        h = (h ^ p[i]) * 1099511628211ULL;
    return h;
}

/* Nonzero when the two keys identify the same mapping. */
static int key_eq(ek_val a, ek_val b)
{
    if (text_key(&a) || text_key(&b)) {
        return text_key(&a) && text_key(&b)
            && ek_str_eq(a.as.ref, b.as.ref);
    }
    return a.tag == b.tag
        && memcmp(&a.as, &b.as, payload_width(&a)) == 0;
}

/* The index of the slot key occupies, or of the free slot where it
   belongs. */
static size_t probe(const ek_dict_entry *entries, size_t capacity, ek_val key)
{
    size_t mask = capacity - 1;
    size_t i = (size_t)hash_key(key) & mask;

    while (entries[i].used && !key_eq(entries[i].key, key))
        i = (i + 1) & mask;
    return i;
}

/* The entry storing key, or NULL when the key is absent. */
static const ek_dict_entry *find_entry(const ek_dict *dict, ek_val key)
{
    size_t i;

    if (dict->count == 0)
        return NULL;
    i = probe(dict->entries, dict->capacity, key);
    return dict->entries[i].used ? &dict->entries[i] : NULL;
}

/* A dictionary object with no table: the empty dictionary. The chain
   of none starts and ends at the no-entry mark. */
static ek_dict *dict_new(void)
{
    ek_dict *dict = ek_rt_alloc_zeroed(sizeof *dict);

    dict->obj.info = &ek_typeinfo_dict;
    dict->obj.rc = 1;
    dict->head = DICT_NO_ENTRY;
    dict->tail = DICT_NO_ENTRY;
    return dict;
}

/* A fresh dictionary whose table holds count entries below the load
   limit: the capacity starts at eight and doubles until it fits. */
static ek_dict *dict_sized_for(size_t count)
{
    ek_dict *dict = dict_new();
    size_t capacity = DICT_MIN_CAP;

    while (count * DICT_LOAD_NUM > capacity * DICT_LOAD_DEN)
        capacity *= 2;
    dict->capacity = capacity;
    dict->entries = ek_rt_alloc_zeroed(capacity * sizeof *dict->entries);
    return dict;
}

/* Retains both values, stores them in the slot key belongs to, and
   appends the slot to the build-order chain. */
static void entry_store(ek_dict *dict, ek_val key, ek_val val)
{
    size_t i = probe(dict->entries, dict->capacity, key);
    ek_dict_entry *slot = &dict->entries[i];

    ek_retain(key);
    ek_retain(val);
    slot->key = key;
    slot->val = val;
    slot->next = DICT_NO_ENTRY;
    slot->used = 1;
    if (dict->tail == DICT_NO_ENTRY)
        dict->head = (uint32_t)i;
    else
        dict->entries[dict->tail].next = (uint32_t)i;
    dict->tail = (uint32_t)i;
    dict->count++;
}

ek_val ek_dict_new(void)
{
    return ek_val_ref(&dict_new()->obj);
}

int ek_dict_get(ek_val d, ek_val key, ek_val *out)
{
    const ek_dict_entry *entry =
        find_entry((const ek_dict *)d.as.ref, key);

    if (entry == NULL)
        return 0;
    *out = entry->val;
    return 1;
}

/* Copies the mappings of a and b into fresh in that order; when
   overwrite is nonzero the mappings of b win on duplicate keys. */
static ek_dict *dict_joined(const ek_dict *a, const ek_dict *b)
{
    ek_dict *fresh = dict_sized_for(a->count + b->count);
    const ek_dict *srcs[2];
    uint32_t idx;
    int side;

    srcs[0] = a;
    srcs[1] = b;
    for (side = 0; side < 2; side++) {
        const ek_dict *src = srcs[side];

        for (idx = src->head; idx != DICT_NO_ENTRY;
             idx = src->entries[idx].next) {
            const ek_dict_entry *e = &src->entries[idx];
            ek_dict_entry *slot =
                &fresh->entries[probe(fresh->entries, fresh->capacity,
                                      e->key)];

            if (slot->used && side == 1) {
                ek_retain(e->val); /* b wins; the slot keeps its place */
                ek_release(slot->val);
                slot->val = e->val;
            } else if (!slot->used) {
                entry_store(fresh, e->key, e->val);
            }
        }
    }
    return fresh;
}

ek_val ek_dict_set(ek_val d, ek_val key, ek_val val)
{
    const ek_dict *old = (const ek_dict *)d.as.ref;
    const ek_dict_entry *existing = find_entry(old, key);
    ek_dict *fresh = dict_sized_for(old->count + (existing == NULL ? 1 : 0));
    uint32_t idx;

    for (idx = old->head; idx != DICT_NO_ENTRY; idx = old->entries[idx].next) {
        const ek_dict_entry *e = &old->entries[idx];

        entry_store(fresh, e->key, e == existing ? val : e->val);
    }
    if (existing == NULL)
        entry_store(fresh, key, val);
    return ek_val_ref(&fresh->obj);
}

ek_val ek_dict_remove(ek_val d, ek_val key)
{
    const ek_dict *old = (const ek_dict *)d.as.ref;
    ek_dict *fresh;
    uint32_t idx;

    if (find_entry(old, key) == NULL)
        return d;
    fresh = dict_sized_for(old->count - 1);
    for (idx = old->head; idx != DICT_NO_ENTRY; idx = old->entries[idx].next) {
        const ek_dict_entry *e = &old->entries[idx];

        if (!key_eq(e->key, key))
            entry_store(fresh, e->key, e->val);
    }
    return ek_val_ref(&fresh->obj);
}

ek_val ek_dict_merge(ek_val a, ek_val b)
{
    return ek_val_ref(&dict_joined((const ek_dict *)a.as.ref,
                                   (const ek_dict *)b.as.ref)->obj);
}

size_t ek_dict_len(ek_val d)
{
    return ((const ek_dict *)d.as.ref)->count;
}

int ek_dict_next(ek_val d, size_t *cursor, ek_val *key, ek_val *val)
{
    const ek_dict *dict = (const ek_dict *)d.as.ref;
    uint32_t idx;

    if (*cursor == 0)
        idx = dict->head;
    else
        idx = dict->entries[*cursor - 1].next;
    if (idx == DICT_NO_ENTRY)
        return 0;
    *key = dict->entries[idx].key;
    *val = dict->entries[idx].val;
    *cursor = (size_t)idx + 1;
    return 1;
}

/* Releases every used entry's key and value, walking the table in
   order; the release path frees the dictionary object itself. */
void ek_dict_drop(ek_obj *obj)
{
    ek_dict *dict = (ek_dict *)obj;
    size_t i;

    for (i = 0; i < dict->capacity; i++) {
        ek_dict_entry *e = &dict->entries[i];

        if (e->used) {
            ek_release(e->key);
            ek_release(e->val);
        }
    }
    free(dict->entries);
}
