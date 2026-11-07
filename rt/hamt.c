#include "internal.h"

#include <string.h>

/* The hash array mapped trie under the dictionaries (dict.c):
   sixteen-way bitmap branches over the key's hash word, four bits per
   level, with the entries whose whole words agree kept in a leaf's
   linear span. An update copies the path it descends and shares every
   node it does not touch, so an insert or a remove costs logarithmic
   new nodes and the old version keeps its mappings. The walks take
   the dictionary for its key domain alone: nodes come in borrowed and
   every node returned is owned. */

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

static void hleaf_drop(ek_obj *o);
static void hbranch_drop(ek_obj *o);

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
        if (ek_dict_key_eq(dict, leaf->trip[i * 3], key))
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
ek_val *ek_ham_find(const ek_dict *dict, ek_val root, unsigned long h,
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

    while (level2 < HAM_LEVELS && nibble(he, level2) == nibble(h, level2))
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
        unsigned long hk = ek_dict_hash_key(dict, keyk);

        kids[nibble(hk, level)] = ek_ham_add(
            dict, kids[nibble(hk, level)], hk, level + 1, keyk,
            leaf->trip[k * 3 + 1], leaf->trip[k * 3 + 2]);
    }
    kids[nib] = ek_ham_add(dict, kids[nib], h, level + 1, key, val, pos);
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
ek_val ek_ham_add(const ek_dict *dict, ek_val root, unsigned long h,
                  unsigned level, ek_val key, ek_val val, ek_val pos)
{
    if (!root.as.ref)
        return leaf1(key, val, pos);
    if (is_leaf(root.as.ref)) {
        const ek_hleaf *leaf = (const ek_hleaf *)root.as.ref;
        uint32_t i;

        for (i = 0; i < leaf->n; i++) {
            if (parts_here(ek_dict_hash_key(dict, leaf->trip[i * 3]), h,
                           level))
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
            ek_ham_add(dict, *at, h, level + 1, key, val, pos));
    }
}

/* The trie with the key's entry's value replaced; the entry is known
   present, and its position stays the entry's own. */
ek_val ek_ham_update(const ek_dict *dict, ek_val root, unsigned long h,
                     unsigned level, ek_val key, ek_val val)
{
    if (is_leaf(root.as.ref)) {
        const ek_hleaf *leaf = (const ek_hleaf *)root.as.ref;
        size_t at = leaf_find(dict, leaf, key) - leaf->trip;
        ek_hleaf *fresh = ek_rt_alloc_zeroed(sizeof *fresh
                                             + leaf->n * 3 * sizeof(ek_val));
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
            ek_ham_update(dict, *at, h, level + 1, key, val));
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
ek_val ek_ham_del(const ek_dict *dict, ek_val root, unsigned long h,
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
        ek_val grown = ek_ham_del(dict, *at, h, level + 1, key);
        uint32_t i = (uint32_t)(at - br->kids);

        if (!grown.as.ref)
            return branch_shrink(br, nib, i);
        return branch_replace(br, i, grown);
    }
}
