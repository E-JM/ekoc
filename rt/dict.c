#include "internal.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* Dictionaries are immutable maps with copy-on-insert: setting a key
   builds a fresh table, and the old dictionary keeps its references.
   Keys are inline scalars, hashed with FNV-1a over the tag byte and
   the payload bytes; the table is open addressing with linear probing
   that grows from eight slots and stays at or under seventy percent
   full, so a probe always finds an empty slot. */

/* The load limit and the starting capacity of the table. */
#define DICT_LOAD_NUM 10
#define DICT_LOAD_DEN 7
#define DICT_MIN_CAP 8

typedef struct {
    ek_val key;
    ek_val val;
    uint8_t used;
} ek_dict_entry;

typedef struct {
    ek_obj obj;
    size_t count;
    size_t capacity;
    ek_dict_entry *entries;
} ek_dict;

/* The payload bytes that identify a scalar: a Char's code is four
   bytes wide, and every other payload fills the whole slot. The rest
   of the slot is not part of the value. */
static size_t payload_width(const ek_val *v)
{
    return v->tag == EK_VAL_CHAR ? sizeof v->as.c : sizeof v->as;
}

/* FNV-1a over the tag byte and the payload bytes, so keys of different
   tags stay distinct even when their payloads coincide. */
static uint64_t hash_key(ek_val v)
{
    uint64_t h = 14695981039346656037ULL;
    const unsigned char *p = (const unsigned char *)&v.as;
    size_t width = payload_width(&v);
    size_t i;

    h = (h ^ (uint64_t)v.tag) * 1099511628211ULL;
    for (i = 0; i < width; i++)
        h = (h ^ p[i]) * 1099511628211ULL;
    return h;
}

/* Nonzero when the two keys carry the same tag and payload. */
static int key_eq(ek_val a, ek_val b)
{
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

/* A dictionary object with no table: the empty dictionary. */
static ek_dict *dict_new(void)
{
    ek_dict *dict = ek_rt_alloc_zeroed(sizeof *dict);

    dict->obj.info = &ek_typeinfo_dict;
    dict->obj.rc = 1;
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

/* Retains both values and stores them in the slot key belongs to. */
static void entry_store(ek_dict *dict, ek_val key, ek_val val)
{
    ek_dict_entry *slot =
        &dict->entries[probe(dict->entries, dict->capacity, key)];

    ek_retain(key);
    ek_retain(val);
    slot->key = key;
    slot->val = val;
    slot->used = 1;
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

ek_val ek_dict_set(ek_val d, ek_val key, ek_val val)
{
    const ek_dict *old = (const ek_dict *)d.as.ref;
    const ek_dict_entry *existing = find_entry(old, key);
    size_t total = old->count + (existing == NULL ? 1 : 0);
    ek_dict *fresh = dict_sized_for(total);
    size_t i;

    for (i = 0; i < old->capacity; i++) {
        const ek_dict_entry *e = &old->entries[i];

        if (!e->used)
            continue;
        if (e == existing)
            entry_store(fresh, e->key, val); /* the mapping is replaced */
        else
            entry_store(fresh, e->key, e->val);
    }
    if (existing == NULL)
        entry_store(fresh, key, val);
    fresh->count = total;
    return ek_val_ref(&fresh->obj);
}

size_t ek_dict_len(ek_val d)
{
    return ((const ek_dict *)d.as.ref)->count;
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
