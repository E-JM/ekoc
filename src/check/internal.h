#ifndef EK_CHECK_INTERNAL_H
#define EK_CHECK_INTERNAL_H

#include <stddef.h>

#include "check/check.h"

/* State shared by the files of this directory. One ek_check call
   checks one compilation root; everything grows in the arena. */

/* Pointer-keyed map (symbol -> scheme, node -> type, type -> type).
   FNV-1a over the pointer bytes; the map layer and the intern table
   share it. */
size_t ck_ptr_hash(const void *p);
typedef struct {
    const void **keys;
    const void **vals;
    size_t count, cap;
} ck_map;

/* A generalized binding type: the annotation plus its rigid
   variables. */
typedef struct {
    const ek_type *type;
    const ek_type **vars; /* EK_T_TVAR nodes */
    size_t nvars;
} ck_scheme;

/* A constructor's payload in its declaration's binders. */
typedef struct {
    const ek_symbol *sym;   /* the constructor symbol */
    const ek_symbol *owner; /* the TYPE symbol */
    const ek_node *variant; /* the EK_N_VARIANT node */
    const ek_type *payload; /* NULL for a zero payload */
} ck_ctor;

/* A declared type's shape. */
typedef struct {
    const ek_symbol *sym;
    const ek_type **binders; /* rigid tvar nodes, positional */
    size_t nbinders;
    ek_type_rhs rhs;
    const ek_node *rec;     /* RECORD/TUPLE rhs node, else NULL */
    const ek_node *decl;    /* the EK_N_TYPE_DECL node */
    const ck_ctor **ctors;  /* one per variant, in declaration order */
    size_t nctors;
} ck_decl;

/* One lexical scope frame of checked bindings; values are generalized
   schemes so an annotated local binding instantiates at its uses. */
typedef struct ck_scope {
    struct ck_scope *parent;
    const char **names;
    ck_scheme **vals;
    size_t count, cap;
} ck_scope;

typedef struct {
    ek_arena *arena;
    ek_diag *diag;
    const ek_resolve_result *in;
    const ek_surface *root; /* in->modules[0] */
    unsigned next_ivar;
    ck_map types;    /* intern table: candidate node -> canonical */
    ck_map schemes;  /* ek_symbol -> ck_scheme */
    ck_map decls;    /* TYPE symbol -> ck_decl */
    ck_map ctors;    /* CONSTRUCTOR symbol -> ck_ctor */
    ck_map pats;     /* constructor pattern node -> selected symbol */
    ck_map firsts;   /* tvar node -> first T_NAME node (E-phantom) */
    ck_map stdsyms;  /* std type name -> the std surface symbol */
    ck_map subst;    /* final substitution memo */
    ck_map bits;     /* node -> own bit: a call's bit, a lambda's body
                        bit, a binding's declared bit (worlds.c) */
    ck_map contrib;  /* node -> OR of call bits in its subtree, lambda
                        interiors excluded (memo) */
    ck_scope *locals;
    int open_scope;  /* inside the body of a declaration whose signature
                        contains the open bit: calls through `?` arrows
                        carry that bit */
    int invert_call; /* the next call carries an implicit callback */
    const ek_type *invert_fn; /* that call's callee type, NULL when opaque */
    const ek_type *invert_cb; /* its callback parameter type */
    int partial_ok;  /* inside a `with` argument: the partial-literal
                        permission extends layer by layer through the
                        record field values it contains */
    ek_ck_row *rows;
    size_t nrows, crows;
    int oom;
} ck_ctx;

/* A type-position scope: the tvars of one annotation. */
typedef struct ck_tvars {
    struct ck_tvars *parent;
    const char **names;
    const ek_type **tvs;
    const ek_node **firsts;
    size_t count, cap;
} ck_tvars;

/* check.c: the entry point, the shared growable buffers and pointer
   maps, and the row table with its diagnostics. store.c: interning
   and the type constructors. subst.c: the closed snapshot, the binder
   substitution, and printable names. */
void *ck_grow(ck_ctx *c, void *old, size_t n, size_t *cap, size_t elem);
void ck_map_put(ck_ctx *c, ck_map *m, const void *key, const void *val);
const void *ck_map_get(const ck_map *m, const void *key);
ek_type *ck_fresh(ck_ctx *c, ek_type_kind kind); /* uninterned */
const ek_type *ck_intern(ck_ctx *c, const ek_type *t); /* canonical */
const ek_type *ck_ivar(ck_ctx *c);
const ek_type *ck_unit(ck_ctx *c);
const ek_type *ck_never(ck_ctx *c);
int ck_is_bottom(const ek_type *t); /* the Never leaf or the std nominal */
const ek_type *ck_nominal(ck_ctx *c, const ek_symbol *sym, const ek_type **args,
                          size_t argc);
const ek_type *ck_nominal_fresh(ck_ctx *c, const ek_symbol *sym);
const ek_type *ck_list(ck_ctx *c, const ek_type *elem);
const ek_type *ck_dict(ck_ctx *c, const ek_type *key, const ek_type *val);
const ek_type *ck_tuple(ck_ctx *c, const ek_type **items, size_t n);
const ek_type *ck_record(ck_ctx *c, ek_tfield *fields, size_t n); /* sorts */
const ek_type *ck_fn(ck_ctx *c, const ek_type **params, size_t n,
                     const ek_type *result, ek_arrow arrow);
const ek_type *ck_prune(const ek_type *t);
const ek_type *ck_subst(ck_ctx *c, const ek_type *t);
/* Replaces each vars[i] with repls[i] through a type. */
const ek_type *ck_apply_binders(ck_ctx *c, const ek_type *t,
                                const ek_type **vars, const ek_type **repls,
                                size_t n);
const char *ck_type_name(ck_ctx *c, const ek_type *t); /* arena text */
void ck_row(ck_ctx *c, const ek_node *n, const ek_type *t, ek_reading r);
void ck_error(ck_ctx *c, const ek_node *n, const char *code, const char *fmt,
              ...);
void ck_warn(ck_ctx *c, const ek_node *n, const char *code, const char *fmt,
             ...);
const char *ck_file(const ck_ctx *c); /* the root module's file */

/* unify.c: occurs check and conformance.
   scheme.c: generalization and instantiation. */
typedef enum {
    CK_OK = 0,      /* types conform */
    CK_NOMINAL,     /* nominal into a different nominal type */
    CK_SHAPE,       /* nominal without the same anonymous shape */
    CK_MISMATCH     /* any other non-conformance */
} ck_conform;
int ck_occurs(const ek_type *v, const ek_type *t);
int ck_bind(ck_ctx *c, const ek_type *v, const ek_type *t);
ck_conform ck_conforms(ck_ctx *c, const ek_type *a, const ek_type *e,
                       int allow_shape);
const char *ck_conform_code(ck_conform k);
const ek_type *ck_unify(ck_ctx *c, const ek_type *a, const ek_type *b,
                        int allow_shape);
ck_scheme ck_generalize(ck_ctx *c, const ek_type *t); /* collects tvars */
const ek_type *ck_instantiate(ck_ctx *c, const ck_scheme *s);
void ck_tvars_collect(ck_ctx *c, const ek_type *t, const ek_type ***out,
                      size_t *n, size_t *cap);
void ck_check_phantom(ck_ctx *c, const ek_node *ann, const ek_type *t);

/* elab.c: annotations and representations. decl.c: declared types,
   their binder scope, and the phantom rule. entry.c: entry rules. */
const ek_type *ck_elaborate(ck_ctx *c, const ek_node *t, ck_tvars *tv);
void ck_tv_add(ck_ctx *c, ck_tvars *tv, const char *name, const ek_type *t,
               const ek_node *first);
void ck_binder_scope(ck_ctx *c, const ck_decl *d, ck_tvars *tv);
const ek_type *ck_decl_repr(ck_ctx *c, const ek_type *nom);
const ek_type *ck_type_of(ck_ctx *c, const ek_symbol *sym);
const ck_decl *ck_decl_of(ck_ctx *c, const ek_symbol *sym);
const ck_ctor *ck_ctor_of(ck_ctx *c, const ek_symbol *sym);
void ck_prebind(ck_ctx *c); /* unannotated top-level rows */
void ck_decls(ck_ctx *c);   /* root type decls, entry rules */

/* expr.c: expression typing. ctl.c: the control forms. */
const ek_type *ck_expr(ck_ctx *c, const ek_node *e, const ek_type *expected);
const ek_type *ck_block(ck_ctx *c, const ek_node *b, const ek_type *expected);
const ek_type *ck_members(ck_ctx *c, const ek_node *b, size_t start,
                          const ek_type *expected);
const ek_type *ck_if(ck_ctx *c, const ek_node *e, const ek_type *expected);
const ek_type *ck_case(ck_ctx *c, const ek_node *e, const ek_type *expected);
void ck_scope_push(ck_ctx *c);
void ck_scope_pop(ck_ctx *c);
void ck_scope_bind(ck_ctx *c, const char *name, const ek_type *t);
void ck_scope_bind_scheme(ck_ctx *c, const char *name, ck_scheme *s);
ck_scheme *ck_scope_get_scheme(const ck_ctx *c, const char *name);
const ek_type *ck_scope_get(const ck_ctx *c, const char *name);

/* Constructor candidates. A reference carries every visible same-named
   constructor; the expected nominal type anchors one, and without an
   anchor only a sole non-generic candidate stands on its own. */
typedef struct {
    const ek_symbol *sym; /* the chosen candidate, NULL when none */
    int anchored;         /* the expected nominal type picked it */
} ck_ctorsel;
ck_ctorsel ck_ctor_select(ck_ctx *c, const ek_node *n, const ek_ref *ref,
                          const ek_type *expected);
const ek_symbol *ck_ctor_pick(const ek_ref *ref, const ek_type *expected);

/* ctor.c: constructor candidate selection in value position. */
const ek_type *ck_ctor_type(ck_ctx *c, const ek_node *n, const ek_ref *ref,
                            const ek_type *expected);

/* literal.c: literal placement and composite literals. */
int ck_is_numeric(const ek_type *t);      /* Int, UInt, Float */
int ck_is_machine_word(const ek_type *t); /* U8..I64 */
int ck_is_std(const ek_type *t, const char *name);
const ek_type *ck_std_type(ck_ctx *c, const char *name);
const ek_symbol *ck_std_symbol(ck_ctx *c, const char *name);
const ek_type *ck_place(ck_ctx *c, const ek_node *lit, const ek_type *expected,
                        int neg);
const ek_type *ck_composite(ck_ctx *c, const ek_node *e,
                            const ek_type *expected, int allow_partial);

/* op.c: operator typing. */
const ek_type *ck_unary(ck_ctx *c, const ek_node *e, const ek_type *expected);
const ek_type *ck_binary(ck_ctx *c, const ek_node *e, const ek_type *expected);

/* worlds.c: the world-bit derivation. Bits are written during
   expression checking; reconcile.c judges them at the annotated
   bindings. */
ek_bit ck_arg_bit(ck_ctx *c, const ek_node *arg, const ek_type *arg_type);
void ck_call_bit(ck_ctx *c, const ek_node *call, ek_bit b);
void ck_lambda_bit(ck_ctx *c, const ek_node *lam, ek_bit b);
void ck_declare_bit(ck_ctx *c, const ek_node *decl, ek_bit b);
ek_bit ck_bit_of(ck_ctx *c, const ek_node *n); /* recorded own bit */
ek_bit ck_body_bit(ck_ctx *c, const ek_node *e); /* subtree OR */
ek_bit ck_call_bit_of(ck_ctx *c, const ek_type *f, const char *member,
                      const ek_type *recv, const ek_node *const *args,
                      const ek_type *const *types, size_t n, size_t off);
int ck_type_opens(const ek_type *t); /* contains an open arrow */
int ck_signature_opens(const ek_type *f); /* own arrow or a feeder */
void ck_fill_bits(ck_ctx *c); /* the final row pass (check.c) */

/* reconcile.c: the no-eq rule and the declared-bit binding rules. */
int ck_is_capability(const ek_type *t); /* the nine capability types */
int ck_no_eq(ck_ctx *c, const ek_type *t); /* the eq derivation halts
                                              here */
void ck_no_eq_error(ck_ctx *c, const ek_node *at, const ek_type *t);
void ck_open_positions(ck_ctx *c, const ek_node *t, int allow);
int ck_annotation_bits(ck_ctx *c, const ek_node *ann, const ek_type *bound);
ek_bit ck_own_bit(const ek_type *bound);
void ck_reconcile(ck_ctx *c, const ek_node *at, const ek_type *bound,
                  const ek_node *val);

/* pattern.c: pattern typing. patbind.c: or-patterns and the
   binding-shape judgments. */
int ck_invert_pattern(const ek_node *pat);
const ek_type *ck_invert_block(ck_ctx *c, const ek_node *m,
                               const ek_node *block, size_t start,
                               const ek_type *expected);
const ek_type *ck_pattern(ck_ctx *c, const ek_node *p, const ek_type *scrut);
const ek_type *ck_or_pattern(ck_ctx *c, const ek_node *p, const ek_type *scrut);
void ck_arm_binds_together(ck_ctx *c, const ek_node *arm); /* a name two
                                        of the arm's patterns bind conflicts */
int ck_irrefutable(const ek_node *p); /* 0 = refutable in a value binding */

/* exhaust.c: coverage of one scrutinee. exhaust_prod.c: the
   multi-scrutinee product and the entry. */
int ck_covers(ck_ctx *c, const ek_type *t, const ek_node *const *pats,
              size_t n);
size_t ck_pat_flatten(ck_ctx *c, const ek_node *p, const ek_node **out,
                      size_t n, size_t cap);
const ek_symbol *ck_pat_head(ck_ctx *c, const ek_node *p);
int ck_pat_catchall(const ek_node *p);
void ck_exhaust(ck_ctx *c, const ek_node *cs, const ek_type **scrut,
                size_t nscrut, const ek_node *const *arms, const int *contrib,
                size_t narms);

/* member.c: receiver-station selection. call.c: calls, lambdas,
   constructor application. */
const ek_type *ck_call(ck_ctx *c, const ek_node *e, const ek_type *expected);
const ek_type *ck_field(ck_ctx *c, const ek_node *e);
const ek_type *ck_link(ck_ctx *c, const ek_node *link, const ek_type *recv,
                       const char *name, ek_reading *rd);
const ek_type *ck_lambda(ck_ctx *c, const ek_node *e, const ek_type *expected);
const ek_type *ck_apply(ck_ctx *c, const ek_node *e, const ek_type *expected);
int ck_ref_at(const ck_ctx *c, const ek_node *n, ek_sym_kind kind,
              const ek_ref **out);

/* extern.c: the compile-time extern and ctype checks over the root's
   declarations. */
void ck_externs(ck_ctx *c);

/* bind.c: the binding forms. */
ck_scheme *ck_mono(ck_ctx *c, const ek_type *t);
const ek_type *ck_stmt(ck_ctx *c, const ek_node *m, int top);

#endif
