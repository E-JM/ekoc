#include "internal.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* Dictionaries are immutable persistent maps over the trie hamt.c
   walks. The observable insertion order is kept by a second structure,
   the order vector: every fresh key is appended at its position and
   the mapping's trie entry carries that position back. Removal writes
   a tombstone into the position and a reinsertion appends again, so a
   walk yields each live mapping once, where it was last inserted,
   with the value it carries now. An update touches only the trie.

   Each dictionary carries the eq and hash hooks its key domain was
   built with, and every derived construction clones the source's
   pair, so one domain keys the same way across a whole build chain.
   The domains the runtime itself can key -- the scalars, the Float
   rules, and the text families -- answer before any hook runs; a
   reference key outside them is answered by the carried pair. */

/* The position a removed key leaves behind: a reference word no value
   carries, retained and released as no reference at all. */
static ek_val tomb(void)
{
    ek_val v;

    v.tag = EK_VAL_REF;
    v.as.ref = NULL;
    return v;
}

static int is_tomb(ek_val v)
{
    return v.tag == EK_VAL_REF && v.as.ref == NULL;
}

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

/* The slot word of a key: the carried hash hook answers the domains
   the runtime cannot key itself; the trie spreads the hook's word
   over its levels. A missing hook is a key domain the trie cannot
   key, not a lookup miss. */
unsigned long ek_dict_hash_key(const ek_dict *dict, ek_val v)
{
    if (text_key(&v))
        return (unsigned long)ek_key_hash(v);
    if (v.tag == EK_VAL_REF && dict->kh)
        return dict->kh(v);
    if (v.tag != EK_VAL_REF)
        return (unsigned long)ek_key_hash(v);
    ek_abort("the dictionary key carries no hash hook");
    return 0;
}

/* Nonzero when the two keys identify the same mapping. The fast
   domains answer first; a reference key outside them goes to the
   carried eq hook. A missing hook is a key domain the trie cannot
   key, not a lookup miss. */
int ek_dict_key_eq(const ek_dict *dict, ek_val a, ek_val b)
{
    if (text_key(&a) || text_key(&b))
        return text_key(&a) && text_key(&b)
            && ek_str_eq(a.as.ref, b.as.ref);
    if (a.tag != b.tag)
        return 0;
    if (a.tag == EK_VAL_REF) {
        if (a.as.ref == NULL || b.as.ref == NULL)
            return a.as.ref == b.as.ref;
        if (dict->keq)
            return dict->keq(a, b);
        ek_abort("the dictionary key carries no equality hook");
    }
    if (a.tag == EK_VAL_FLOAT)
        return a.as.f == b.as.f;
    return memcmp(&a.as, &b.as, payload_width(&a)) == 0;
}

/* A dictionary object over the trie and order vector given, both
   owned by the object from here. */
static ek_val dict_of(ek_key_eq_fn keq, ek_key_hash_fn kh, ek_val root,
                      ek_val order, size_t count)
{
    ek_dict *dict = ek_rt_alloc_zeroed(sizeof *dict);

    dict->obj.info = &ek_typeinfo_dict;
    dict->obj.rc = 1;
    dict->keq = keq;
    dict->kh = kh;
    dict->root = root;
    dict->order = order;
    dict->count = count;
    return ek_val_ref(&dict->obj);
}

ek_val ek_dict_new(ek_key_eq_fn keq, ek_key_hash_fn kh)
{
    return dict_of(keq, kh, ek_val_unit(), ek_vec_new(), 0);
}

int ek_dict_get(ek_val d, ek_val key, ek_val *out)
{
    const ek_dict *dict = (const ek_dict *)d.as.ref;
    ek_val *e = ek_ham_find(dict, dict->root,
                            ek_dict_hash_key(dict, key), 0, key);

    if (e == NULL)
        return 0;
    *out = e[1];
    return 1;
}

ek_val ek_dict_set(ek_val d, ek_val key, ek_val val)
{
    const ek_dict *old = (const ek_dict *)d.as.ref;
    unsigned long h = ek_dict_hash_key(old, key);
    ek_val *e = ek_ham_find(old, old->root, h, 0, key);

    /* An insert whose key equals no key, not even itself, changes
       nothing: the entry could never be found again (Clause 9.10.4). */
    if (!ek_dict_key_eq(old, key, key)) {
        ek_retain(d);
        return d;
    }
    if (e) {
        ek_val root = ek_ham_update(old, old->root, h, 0, key, val);

        ek_retain(old->order);
        return dict_of(old->keq, old->kh, root, old->order, old->count);
    }
    {
        ek_val pos = ek_val_int((int64_t)ek_vec_len(old->order));
        ek_val root = ek_ham_add(old, old->root, h, 0, key, val, pos);
        ek_val order = ek_vec_push(old->order, key);

        return dict_of(old->keq, old->kh, root, order, old->count + 1);
    }
}

ek_val ek_dict_remove(ek_val d, ek_val key)
{
    const ek_dict *old = (const ek_dict *)d.as.ref;
    unsigned long h = ek_dict_hash_key(old, key);
    ek_val *e = ek_ham_find(old, old->root, h, 0, key);

    if (!e) {
        ek_retain(d);
        return d;
    }
    {
        ek_val root = ek_ham_del(old, old->root, h, 0, key);
        ek_obj *order = ek_vec_set(old->order, e[2], tomb());

        return dict_of(old->keq, old->kh, root, ek_val_ref(order),
                       old->count - 1);
    }
}

ek_val ek_dict_merge(ek_val a, ek_val b)
{
    ek_val out = a;
    ek_dict_iter it;
    ek_val k, v;

    ek_retain(out);
    ek_dict_iter_begin(&it, b);
    while (ek_dict_iter_next(&it, &k, &v)) {
        ek_val next = ek_dict_set(out, k, v);

        ek_release(out);
        out = next;
    }
    ek_dict_iter_end(&it);
    return out;
}

size_t ek_dict_len(ek_val d)
{
    return ((const ek_dict *)d.as.ref)->count;
}

/* The live mapping at a walk step: the order vector names the key and
   the position, and the trie's entry answers only when it still
   carries that position, so a tombstoned or reinserted position
   yields nothing. */
int ek_dict_iter_next(ek_dict_iter *it, ek_val *key, ek_val *val)
{
    const ek_dict *dict = (const ek_dict *)it->hold.as.ref;
    size_t slot;
    ek_val k;

    while (ek_vec_iter_next(&it->slots, &slot, &k)) {
        ek_val *e;

        if (is_tomb(k))
            continue;
        e = ek_ham_find(dict, dict->root, ek_dict_hash_key(dict, k), 0, k);
        if (e && e[2].as.i == (int64_t)slot) {
            *key = k;
            *val = e[1];
            return 1;
        }
    }
    return 0;
}

void ek_dict_iter_begin(ek_dict_iter *it, ek_val d)
{
    it->hold = d;
    ek_retain(d);
    ek_vec_iter_begin(&it->slots, ((const ek_dict *)d.as.ref)->order);
}

void ek_dict_iter_end(ek_dict_iter *it)
{
    ek_vec_iter_end(&it->slots);
    ek_release(it->hold);
    it->hold = ek_val_unit();
}

/* The traversals run as native loops over the walk, so their stack
   usage is independent of the entry count. The entries are borrowed:
   the walk holds the dictionary for the caller, and each callback
   retains whatever it stores. */

/* The left-to-right fold in insertion order: the accumulator starts at
   acc, each step calls f with the accumulator, the key, and the value,
   and the last step's result is returned. acc is borrowed -- the
   caller keeps its own reference -- and the returned accumulator is
   the caller's to release. */
ek_val ek_dict_fold(ek_val d, ek_val acc, ek_val f)
{
    ek_dict_iter it;
    ek_val k, x;

    ek_retain(acc);
    ek_dict_iter_begin(&it, d);
    while (ek_dict_iter_next(&it, &k, &x)) {
        ek_val args[3];
        ek_val out;

        args[0] = acc;
        args[1] = k;
        args[2] = x;
        out = ek_closure_call(f, args, 3);
        ek_release(acc);
        acc = out;
    }
    ek_dict_iter_end(&it);
    return acc;
}

/* The callback over every mapping in insertion order; the results are
   dropped and the unit value returned. */
ek_val ek_dict_each(ek_val d, ek_val f)
{
    ek_dict_iter it;
    ek_val k, x;

    ek_dict_iter_begin(&it, d);
    while (ek_dict_iter_next(&it, &k, &x)) {
        ek_val args[2];
        ek_val out;

        args[0] = k;
        args[1] = x;
        out = ek_closure_call(f, args, 2);
        ek_release(out);
    }
    ek_dict_iter_end(&it);
    return ek_val_unit();
}

/* A fresh dictionary whose values are the callback's results, the keys
   and their insertion order unchanged. */
ek_val ek_dict_map(ek_val d, ek_val f)
{
    const ek_dict *old = (const ek_dict *)d.as.ref;
    ek_val out = ek_dict_new(old->keq, old->kh);
    ek_dict_iter it;
    ek_val k, x;

    ek_dict_iter_begin(&it, d);
    while (ek_dict_iter_next(&it, &k, &x)) {
        ek_val args[2];
        ek_val next;

        args[0] = k;
        args[1] = x;
        next = ek_dict_set(out, k, ek_closure_call(f, args, 2));
        ek_release(out);
        out = next;
    }
    ek_dict_iter_end(&it);
    return out;
}

/* A fresh dictionary holding the mappings whose callback says keep, in
   insertion order. */
ek_val ek_dict_filter(ek_val d, ek_val f)
{
    const ek_dict *old = (const ek_dict *)d.as.ref;
    ek_val out = ek_dict_new(old->keq, old->kh);
    ek_dict_iter it;
    ek_val k, x;

    ek_dict_iter_begin(&it, d);
    while (ek_dict_iter_next(&it, &k, &x)) {
        ek_val args[2];
        ek_val keep;

        args[0] = k;
        args[1] = x;
        keep = ek_closure_call(f, args, 2);
        if (keep.tag == EK_VAL_INT && keep.as.i != 0) {
            ek_val next = ek_dict_set(out, k, x);

            ek_release(out);
            out = next;
        }
        ek_release(keep);
    }
    ek_dict_iter_end(&it);
    return out;
}

/* Releases the trie and the order vector; the release path frees the
   dictionary object itself. */
void ek_dict_drop(ek_obj *obj)
{
    ek_dict *dict = (ek_dict *)obj;

    ek_release(dict->root);
    ek_release(dict->order);
}
