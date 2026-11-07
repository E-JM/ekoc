#include "internal.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* Dictionaries are immutable persistent maps. The body is a hash
   array mapped trie: sixteen-way bitmap branches over the key's hash
   word, four bits per level, with the entries whose whole words agree
   kept in a leaf's linear span. An update copies the path it descends
   and shares every node it does not touch, so an insert or a remove
   costs logarithmic new nodes and the old version keeps its mappings.

   The observable insertion order is kept by a second structure, the
   order vector: every fresh key is appended at its position and the
   mapping's trie entry carries that position back. Removal writes a
   tombstone into the position and a reinsertion appends again, so a
   walk yields each live mapping once, where it was last inserted,
   with the value it carries now. An update touches only the trie.

   Each dictionary carries the eq and hash hooks its key domain was
   built with, and every derived construction clones the source's
   pair, so one domain keys the same way across a whole build chain.
   The domains the runtime itself can key -- the scalars, the Float
   rules, and the text families -- answer before any hook runs; a
   reference key outside them is answered by the carried pair. */

/* The hash word's nibble width and the branch fanout it names. */
#define HAM_BITS 4
#define HAM_WAY 16
#define HAM_LEVELS (sizeof(unsigned long) * 8 / HAM_BITS)

/* A trie leaf: the entries whose hash words agree through every
   nibble, as key, value, position triplets. */
typedef struct {
    ek_obj obj;
    uint32_t n;
    ek_val trip[];
} ek_hleaf;

/* A trie branch: the populated children a bitmap names, each a leaf
   or a branch, four bits of hash per level. */
typedef struct {
    ek_obj obj;
    uint32_t map;
    ek_val kids[];
} ek_hbranch;

typedef struct {
    ek_obj obj;
    size_t count;   /* the live mappings */
    ek_val root;    /* the trie's root node, unit when empty */
    ek_val order;   /* the order vector of keys and tombstones */
    ek_key_eq_fn keq;
    ek_key_hash_fn kh;
} ek_dict;

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

static void hleaf_drop(ek_obj *o);
static void hbranch_drop(ek_obj *o);
static ek_val ham_add(const ek_dict *dict, ek_val root, unsigned long h,
                      unsigned level, ek_val key, ek_val val, ek_val pos);

static const ek_typeinfo ek_typeinfo_hleaf = { EK_KIND_DICT, hleaf_drop,
                                               NULL, NULL };
static const ek_typeinfo ek_typeinfo_hbranch = { EK_KIND_DICT, hbranch_drop,
                                                 NULL, NULL };

static ek_val node_val(ek_obj *o)
{
    return ek_val_ref(o);
}

static void hleaf_drop(ek_obj *o)
{
    ek_hleaf *leaf = (ek_hleaf *)o;
    uint32_t i;

    for (i = 0; i < leaf->n * 3; i++)
        ek_release(leaf->trip[i]); /* the position words are no refs */
}

static void hbranch_drop(ek_obj *o)
{
    ek_hbranch *br = (ek_hbranch *)o;
    uint32_t n = __builtin_popcount(br->map);
    uint32_t i;

    for (i = 0; i < n; i++)
        ek_release(br->kids[i]);
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
static unsigned long hash_key(const ek_dict *dict, ek_val v)
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
static int key_eq(const ek_dict *dict, ek_val a, ek_val b)
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

static int is_leaf(ek_obj *o)
{
    return o->info == &ek_typeinfo_hleaf;
}

/* A fresh leaf over one triplet, the key and value retained. */
static ek_val leaf1(ek_val key, ek_val val, ek_val pos)
{
    ek_hleaf *leaf = ek_rt_alloc_zeroed(sizeof *leaf + 3 * sizeof(ek_val));

    leaf->obj.info = &ek_typeinfo_hleaf;
    leaf->obj.rc = 1;
    leaf->n = 1;
    ek_retain(key);
    ek_retain(val);
    leaf->trip[0] = key;
    leaf->trip[1] = val;
    leaf->trip[2] = pos;
    return node_val(&leaf->obj);
}

/* The leaf's entry of the key, or NULL. */
static ek_val *leaf_find(const ek_dict *dict, const ek_hleaf *leaf,
                         ek_val key)
{
    uint32_t i;

    for (i = 0; i < leaf->n; i++) {
        if (key_eq(dict, leaf->trip[i * 3], key))
            return (ek_val *)&leaf->trip[i * 3];
    }
    return NULL;
}

/* The child a branch holds at the nibble, or NULL. */
static ek_val *branch_at(ek_hbranch *br, unsigned nib)
{
    uint32_t bit = 1u << nib;

    if (!(br->map & bit))
        return NULL;
    return &br->kids[__builtin_popcount(br->map & (bit - 1))];
}

/* The nibble of the hash word at the level. */
static unsigned nibble(unsigned long h, unsigned level)
{
    return (h >> (level * HAM_BITS)) & (HAM_WAY - 1);
}

/* A fresh branch of the same map whose slot at index i holds moved
   instead: every other child is retained from the source, and moved
   itself passes its ownership into the array. */
static ek_val branch_replace(const ek_hbranch *br, uint32_t i,
                             ek_val moved)
{
    uint32_t n = __builtin_popcount(br->map);
    ek_hbranch *fresh = ek_rt_alloc_zeroed(sizeof *fresh
                                           + n * sizeof(ek_val));
    uint32_t k;

    fresh->obj.info = &ek_typeinfo_hbranch;
    fresh->obj.rc = 1;
    fresh->map = br->map;
    for (k = 0; k < n; k++) {
        fresh->kids[k] = br->kids[k];
        if (k != i)
            ek_retain(fresh->kids[k]);
    }
    fresh->kids[i] = moved;
    return node_val(&fresh->obj);
}

/* A fresh branch with one child more, at the nibble, holding moved. */
static ek_val branch_extend(const ek_hbranch *br, unsigned nib, ek_val moved)
{
    uint32_t n = __builtin_popcount(br->map);
    uint32_t slot = __builtin_popcount(br->map & ((1u << nib) - 1));
    ek_hbranch *fresh = ek_rt_alloc_zeroed(sizeof *fresh
                                           + (n + 1) * sizeof(ek_val));
    uint32_t k;

    fresh->obj.info = &ek_typeinfo_hbranch;
    fresh->obj.rc = 1;
    fresh->map = br->map | (1u << nib);
    memcpy(fresh->kids, br->kids, slot * sizeof(ek_val));
    memcpy(fresh->kids + slot + 1, br->kids + slot,
           (n - slot) * sizeof(ek_val));
    for (k = 0; k <= n; k++) {
        if (k != slot)
            ek_retain(fresh->kids[k]);
    }
    fresh->kids[slot] = moved;
    return node_val(&fresh->obj);
}

/* The leaf's entry of the key, or NULL, over the trie from the root. */
static ek_val *ham_find(const ek_dict *dict, ek_val root, unsigned long h,
                        unsigned level, ek_val key)
{
    while (root.as.ref) {
        if (is_leaf(root.as.ref))
            return leaf_find(dict, (const ek_hleaf *)root.as.ref, key);
        {
            ek_val *kid = branch_at((ek_hbranch *)root.as.ref,
                                    nibble(h, level));

            if (!kid)
                return NULL;
            root = *kid;
            level++;
        }
    }
    return NULL;
}

/* Nonzero when the entry's hash word parts from the new key's exactly
   at the level: their nibbles agree through it and disagree on it. */
static int parts_here(unsigned long he, unsigned long h, unsigned level)
{
    unsigned level2 = level;

    while (level2 < HAM_LEVELS
           && nibble(he, level2) == nibble(h, level2))
        level2++;
    return level2 == level && level2 < HAM_LEVELS;
}

/* The branch that takes a leaf parting from the new key at the level:
   every entry and the new one drop one level, each into the sub-trie
   of its own nibble, so a sub-trie's members agree through this level
   and their insertions at level + 1 key on their own. */
static ek_val leaf_split(const ek_dict *dict, const ek_hleaf *leaf,
                         unsigned long h, unsigned level, ek_val key,
                         ek_val val, ek_val pos)
{
    ek_val kids[HAM_WAY];
    ek_hbranch *fresh;
    unsigned nib = nibble(h, level);
    uint32_t k, slot = 0;
    uint32_t map = 0;

    for (k = 0; k < HAM_WAY; k++)
        kids[k] = ek_val_unit();
    for (k = 0; k < leaf->n; k++) {
        ek_val keyk = leaf->trip[k * 3];
        unsigned long hk = hash_key(dict, keyk);

        kids[nibble(hk, level)] = ham_add(
            dict, kids[nibble(hk, level)], hk, level + 1, keyk,
            leaf->trip[k * 3 + 1], leaf->trip[k * 3 + 2]);
    }
    kids[nib] = ham_add(dict, kids[nib], h, level + 1, key, val, pos);
    for (k = 0; k < HAM_WAY; k++) {
        if (kids[k].as.ref)
            map |= 1u << k;
    }
    fresh = ek_rt_alloc_zeroed(sizeof *fresh
                               + __builtin_popcount(map) * sizeof(ek_val));
    fresh->obj.info = &ek_typeinfo_hbranch;
    fresh->obj.rc = 1;
    fresh->map = map;
    for (k = 0; k < HAM_WAY; k++) {
        if (kids[k].as.ref)
            fresh->kids[slot++] = kids[k];
    }
    return node_val(&fresh->obj);
}

/* The leaf with the new triplet appended: every entry agrees with the
   new key through this level, so the linear span takes one more. */
static ek_val leaf_append(const ek_hleaf *leaf, ek_val key, ek_val val,
                          ek_val pos)
{
    ek_hleaf *fresh = ek_rt_alloc_zeroed(
        sizeof *fresh + (leaf->n + 1) * 3 * sizeof(ek_val));
    uint32_t k;

    fresh->obj.info = &ek_typeinfo_hleaf;
    fresh->obj.rc = 1;
    fresh->n = leaf->n + 1;
    for (k = 0; k < leaf->n * 3; k++) {
        fresh->trip[k] = leaf->trip[k];
        if (k % 3 != 2)
            ek_retain(fresh->trip[k]);
    }
    ek_retain(key);
    ek_retain(val);
    fresh->trip[leaf->n * 3] = key;
    fresh->trip[leaf->n * 3 + 1] = val;
    fresh->trip[leaf->n * 3 + 2] = pos;
    return node_val(&fresh->obj);
}

/* The trie with the key's entry added; the key is known absent. All
   inputs are borrowed and the returned root is owned. A leaf whose
   entries part from the new key at this level splits under a branch;
   entries whose whole word agrees with the new key share the leaf's
   linear span. */
static ek_val ham_add(const ek_dict *dict, ek_val root, unsigned long h,
                      unsigned level, ek_val key, ek_val val, ek_val pos)
{
    if (!root.as.ref)
        return leaf1(key, val, pos);
    if (is_leaf(root.as.ref)) {
        const ek_hleaf *leaf = (const ek_hleaf *)root.as.ref;
        uint32_t i;

        for (i = 0; i < leaf->n; i++) {
            if (parts_here(hash_key(dict, leaf->trip[i * 3]), h, level))
                return leaf_split(dict, leaf, h, level, key, val, pos);
        }
        return leaf_append(leaf, key, val, pos);
    }
    {
        const ek_hbranch *br = (const ek_hbranch *)root.as.ref;
        unsigned nib = nibble(h, level);
        ek_val *at = branch_at((ek_hbranch *)br, nib);

        if (!at)
            return branch_extend(br, nib, leaf1(key, val, pos));
        return branch_replace(
            br, (uint32_t)(at - br->kids),
            ham_add(dict, *at, h, level + 1, key, val, pos));
    }
}

/* The trie with the key's entry's value replaced; the entry is known
   present, and its position stays the entry's own. */
static ek_val ham_update(const ek_dict *dict, ek_val root, unsigned long h,
                         unsigned level, ek_val key, ek_val val)
{
    if (is_leaf(root.as.ref)) {
        const ek_hleaf *leaf = (const ek_hleaf *)root.as.ref;
        size_t at = leaf_find(dict, leaf, key) - leaf->trip;
        ek_hleaf *fresh = ek_rt_alloc_zeroed(sizeof *fresh
                                             + leaf->n * 3
                                                   * sizeof(ek_val));
        uint32_t k;

        fresh->obj.info = &ek_typeinfo_hleaf;
        fresh->obj.rc = 1;
        fresh->n = leaf->n;
        for (k = 0; k < leaf->n * 3; k++) {
            fresh->trip[k] = leaf->trip[k];
            if (k != at + 1 && k % 3 != 2)
                ek_retain(fresh->trip[k]);
        }
        ek_retain(val);
        fresh->trip[at + 1] = val;
        return node_val(&fresh->obj);
    }
    {
        const ek_hbranch *br = (const ek_hbranch *)root.as.ref;
        ek_val *at = branch_at((ek_hbranch *)br, nibble(h, level));

        return branch_replace(
            br, (uint32_t)(at - br->kids),
            ham_update(dict, *at, h, level + 1, key, val));
    }
}

/* The leaf without its one triplet: the entries before it keep their
   slots, the entries after it shift down three, and an only child
   leaves nothing. */
static ek_val leaf_del(const ek_hleaf *leaf, size_t at)
{
    ek_hleaf *fresh;
    uint32_t k;

    if (leaf->n == 1)
        return ek_val_unit();
    fresh = ek_rt_alloc_zeroed(sizeof *fresh
                               + (leaf->n - 1) * 3 * sizeof(ek_val));
    fresh->obj.info = &ek_typeinfo_hleaf;
    fresh->obj.rc = 1;
    fresh->n = leaf->n - 1;
    for (k = 0; k < leaf->n * 3; k++) {
        if (k >= at && k < at + 3)
            continue;
        fresh->trip[k < at ? k : k - 3] = leaf->trip[k];
        if (k % 3 != 2)
            ek_retain(leaf->trip[k]);
    }
    return node_val(&fresh->obj);
}

/* The branch without the one child at slot i: an only child gives way
   to nothing, a wider map keeps the rest in their order. */
static ek_val branch_shrink(const ek_hbranch *br, unsigned nib, uint32_t i)
{
    uint32_t n = __builtin_popcount(br->map);
    ek_hbranch *fresh;
    uint32_t k, keep = 0;

    if (n == 1)
        return ek_val_unit();
    fresh = ek_rt_alloc_zeroed(sizeof *fresh + (n - 1) * sizeof(ek_val));
    fresh->obj.info = &ek_typeinfo_hbranch;
    fresh->obj.rc = 1;
    fresh->map = br->map & ~(1u << nib);
    for (k = 0; k < n; k++) {
        if (k == i)
            continue;
        fresh->kids[keep] = br->kids[k];
        ek_retain(fresh->kids[keep]);
        keep++;
    }
    return node_val(&fresh->obj);
}

/* The trie without the key's entry; the key is known present. A
   branch left with no child of its own gives way to nothing here. */
static ek_val ham_del(const ek_dict *dict, ek_val root, unsigned long h,
                      unsigned level, ek_val key)
{
    if (is_leaf(root.as.ref)) {
        const ek_hleaf *leaf = (const ek_hleaf *)root.as.ref;

        return leaf_del(leaf, (size_t)(leaf_find(dict, leaf, key)
                                       - leaf->trip));
    }
    {
        const ek_hbranch *br = (const ek_hbranch *)root.as.ref;
        unsigned nib = nibble(h, level);
        ek_val *at = branch_at((ek_hbranch *)br, nib);
        ek_val grown = ham_del(dict, *at, h, level + 1, key);
        uint32_t i = (uint32_t)(at - br->kids);

        if (!grown.as.ref)
            return branch_shrink(br, nib, i);
        return branch_replace(br, i, grown);
    }
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
    return node_val(&dict->obj);
}

ek_val ek_dict_new(ek_key_eq_fn keq, ek_key_hash_fn kh)
{
    return dict_of(keq, kh, ek_val_unit(), ek_vec_new(), 0);
}

int ek_dict_get(ek_val d, ek_val key, ek_val *out)
{
    const ek_dict *dict = (const ek_dict *)d.as.ref;
    ek_val *e = ham_find(dict, dict->root, hash_key(dict, key), 0, key);

    if (e == NULL)
        return 0;
    *out = e[1];
    return 1;
}

ek_val ek_dict_set(ek_val d, ek_val key, ek_val val)
{
    const ek_dict *old = (const ek_dict *)d.as.ref;
    unsigned long h = hash_key(old, key);
    ek_val *e = ham_find(old, old->root, h, 0, key);

    /* An insert whose key equals no key, not even itself, changes
       nothing: the entry could never be found again (Clause 9.10.4). */
    if (!key_eq(old, key, key)) {
        ek_retain(d);
        return d;
    }
    if (e) {
        ek_val root = ham_update(old, old->root, h, 0, key, val);

        ek_retain(old->order);
        return dict_of(old->keq, old->kh, root, old->order, old->count);
    }
    {
        ek_val pos = ek_val_int((int64_t)ek_vec_len(old->order));
        ek_val root = ham_add(old, old->root, h, 0, key, val, pos);
        ek_val order = ek_vec_push(old->order, key);

        return dict_of(old->keq, old->kh, root, order, old->count + 1);
    }
}

ek_val ek_dict_remove(ek_val d, ek_val key)
{
    const ek_dict *old = (const ek_dict *)d.as.ref;
    unsigned long h = hash_key(old, key);
    ek_val *e = ham_find(old, old->root, h, 0, key);

    if (!e) {
        ek_retain(d);
        return d;
    }
    {
        ek_val root = ham_del(old, old->root, h, 0, key);
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
        e = ham_find(dict, dict->root, hash_key(dict, k), 0, k);
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
