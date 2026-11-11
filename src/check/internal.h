#ifndef EK_CHECK_INTERNAL_H
#define EK_CHECK_INTERNAL_H

#include <stddef.h>

#include "check/check.h"
#include "support/surface.h"

/* State shared by the files of this directory. One ek_check call
   checks every module of the closure, root first; everything grows in
   the arena. */

/* Pointer-keyed map (symbol -> scheme, node -> type, type -> type).
   FNV-1a over the pointer bytes; the map layer and the intern table
   share it. */
size_t ck_ptr_hash(const void *p);
typedef struct {
    const void **keys;
    const void **vals;
    size_t count, cap;
} ck_map;

/* One service obligation a body leaves: a derived service that can
   halt, used on a type the body does not determine, with the use site
   and its module for the diagnostic. */
typedef struct {
    const char *service;
    const ek_type *type;
    const ek_node *at;
    const ek_surface *mod;
} ck_req;

/* The obligations of one scheme, shared by every copy of it and grown
   while its body is checked. */
typedef struct {
    ck_req *items;
    size_t n, cap;
} ck_reqs;

/* A generalized binding type: the annotation plus its rigid
   variables. A bridge scheme is compiler-injected machinery: its
   variables' undetermined resting state is opacity, not an error. */
typedef struct {
    const ek_type *type;
    const ek_type **vars; /* EK_T_TVAR nodes */
    size_t nvars;
    int bridge;
    ck_reqs *reqs;
} ck_scheme;

/* One instantiation recorded at a use site: the callee scheme at the
   actuals its variables took, written in the variables of the body
   being checked (the edge's caller). A caller with no variables is a
   seed of the instance-set simulation. */
typedef struct {
    const ck_scheme *caller;      /* NULL in a body with no declaration */
    const ck_scheme *callee;
    const char *name;             /* callee spelling, for the diagnostic */
    const ek_surface *mod;        /* the module of the use site */
    const ek_type **caller_vars;  /* caller scheme vars, scheme order */
    size_t ncaller_vars;
    const ek_type **args;         /* one actual per callee scheme var */
    size_t nargs;
    const ek_node *at;            /* the call node */
    size_t done;                  /* obligations already checked */
} ck_inst_edge;

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

/* One composite literal awaiting its family: the node, the module it
   was checked in, and its file for the family-less report. */
typedef struct {
    const ek_node *node;
    const ek_surface *mod;
    const char *file;
} ck_pend_lit;

typedef struct {
    ek_arena *arena;
    ek_diag *diag;
    const ek_resolve_result *in;
    const ek_surface *root; /* in->modules[0], the entry module */
    const ek_surface *cur;  /* the module whose members are checked */
    unsigned next_ivar;
    ck_map opaque;   /* bridge-marked inference variables */
    int inst_bridge; /* the scheme being instantiated is injected */
    ck_map types;    /* intern table: candidate node -> canonical */
    ck_map schemes;  /* ek_symbol -> ck_scheme */
    ck_map decls;    /* TYPE symbol -> ck_decl */
    ck_map ctors;    /* CONSTRUCTOR symbol -> ck_ctor */
    ck_map pats;     /* constructor pattern node -> selected symbol */
    ck_map firsts;   /* tvar node -> first T_NAME node (E-phantom) */
    ck_map stdsyms;  /* std type name -> the std surface symbol */
    struct ck_synth_row { /* the synthesized std symbols, keyed by
                             spelling: the name reaches several call
                             sites as distinct literals, and the map
                             above is keyed by pointer */
        const char *name;
        const ek_symbol *sym;
    } *synth;
    size_t nsynth, csynth;
    ck_map subst;    /* final substitution memo */
    ck_map absent;   /* link node -> reported member absence */
    ck_map bits;     /* node -> own bit: a call's bit, a lambda's body
                        bit, a binding's declared bit (worlds.c) */
    ck_map contrib;  /* node -> OR of call bits in its subtree, lambda
                        interiors excluded (memo) */
    const char **shared; /* type names more than one module declares,
                            built on first use (subst.c) */
    size_t nshared, cshared;
    int shared_ready;
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
    int opaque_arg;  /* inside an argument the callee cannot type: its
                        literals take the opaque reading without a
                        pending sweep entry */
    ek_ck_row *rows;
    size_t nrows, crows;
    /* Composite literals placed under an undetermined expectation: the
       node with the file it was checked in, reported by the final
       sweep when substitution left the family undecided. */
    ck_pend_lit *pend_lits;
    size_t npend, cpend;
    /* Set while the final sweep re-runs a decided literal's placement:
       expressions answer the types their rows already carry, and no
       new rows record. */
    int sweep_recheck;
    /* The instantiation graph: the body being checked, its rigid
       variables, the call marked as the current callee's, and the
       instantiate capture of its scheme variables. */
    const ck_scheme *inst_decl;
    const ek_type **inst_vars;
    size_t inst_nvars;
    int inst_body;
    const ek_node *inst_call;
    ck_map *inst_capture;
    ck_inst_edge *inst_edges;
    size_t ninst, cinst;
    /* Every service obligation recorded in a checked body, in check
       order: the ones the module pass left ground are decided after
       it, the rest travel with their scheme to its uses. */
    ck_req *reqs;
    size_t nreqs, creqs;
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
   maps. rows.c: the row table, its diagnostics, and its final
   finishing. store.c: interning and the type constructors. subst.c:
   the closed snapshot, the binder substitution, and printable names. */
void *ck_grow(ck_ctx *c, void *old, size_t n, size_t *cap, size_t elem);
void ck_map_put(ck_ctx *c, ck_map *m, const void *key, const void *val);
const void *ck_map_get(const ck_map *m, const void *key);
ek_type *ck_fresh(ck_ctx *c, ek_type_kind kind); /* uninterned */
const ek_type *ck_intern(ck_ctx *c, const ek_type *t); /* canonical */
const ek_type *ck_ivar(ck_ctx *c);
const ek_type *ck_opaque(ck_ctx *c); /* a bridge-marked inference variable */
int ck_ivar_opaque(const ck_ctx *c, const ek_type *t);
const ek_type *ck_unit(ck_ctx *c);
const ek_type *ck_never(ck_ctx *c);
int ck_is_bottom(const ek_type *t); /* the `Never` leaf */

/* One injected pair of the capability module: the unwrap and wrap of
   one session bundle, by the `_`-prefixed name the cap source
   references without declaring. */
typedef struct {
    const char *name;
    const ek_sig *sig;
} ns_pair_row;

extern const ns_pair_row ck_ns_pair_rows[];
extern const size_t ck_ns_pair_rows_len;

/* A soft walk-depth guard: pattern structure is finite, and a state
   this deep is pathological input. */
#define EXH_MAX_DEPTH 512

typedef struct {
    const ek_type **cols;
    size_t ncols;
    const ek_node ***rows; /* one contribution vector per row */
    size_t nrows;
} ex_state;

typedef struct {
    ex_state snap; /* the copied pointer arrays; vectors are shared */
    int covered;
} ex_entry;

typedef struct {
    ex_entry *entries;
    size_t count, cap;
} ex_memo;

/* A scratch buffer of flattened alternatives, grown to fit. */
typedef struct {
    const ek_node **v;
    size_t cap;
} ex_buf;

/* The derived rows of one branch. */
typedef struct {
    const ek_node ***v;
    size_t n, cap;
} ex_rows;

/* exhaust_prod.c: the exhaustiveness walk's shared state. The branch
   functions split one column; the growth helpers build the child
   state's rows and vectors. */
size_t ex_flat_into(ck_ctx *c, const ek_node *p, ex_buf *b);
const ek_node **ex_vec_at(ck_ctx *c, const ek_node **v, size_t ncols,
                          size_t pos, const ek_node *const *ins,
                          size_t nins);
const ek_type **ex_cols_at(ck_ctx *c, const ek_type **cols, size_t ncols,
                           size_t pos, const ek_type *const *ins,
                           size_t nins);
void ex_rows_add(ck_ctx *c, ex_rows *r, const ek_node **vec);
/* One surviving row answered with wildcards, and one branch's child
   state walked at the same position. */
void ex_rows_wild(ck_ctx *c, ex_rows *r, const ek_node **row,
                  size_t ncols, size_t pos, const ek_node **wild, size_t k);
int ex_step(ck_ctx *c, const ex_state *st, size_t pos, ex_memo *memo,
            int depth, const ek_type *const *ins, size_t nins,
            const ex_rows *rows);
/* exhaust_prod.c: one walk step, recursive into the branches. */
int covers_walk(ck_ctx *c, ex_state *st, size_t pos, ex_memo *memo,
                int depth);

int ex_ctor_branch(ck_ctx *c, ex_state *st, size_t pos, ex_memo *memo,
                   int depth, const ck_ctor *ct);
int ex_shape_branch(ck_ctx *c, ex_state *st, size_t pos, ex_memo *memo,
                    int depth, const ek_type *t);
int ex_list_branch(ck_ctx *c, ex_state *st, size_t pos, ex_memo *memo,
                   int depth);
int ex_scalar_branch(ck_ctx *c, ex_state *st, size_t pos, ex_memo *memo,
                     int depth);

/* namespace.c: the compiler namespace rows and the sig builder.
   module_sym is the surface type-symbol scan the std lookup of
   literal.c shares. */
const ek_symbol *module_sym(ck_ctx *c, const char *module, const char *name);
const ek_type *ck_ns_row(ck_ctx *c, const ek_type *recv, const char *name,
                         int *owned); /* a compiler namespace row, or NULL */
int ck_ns_owned(const ek_type *recv); /* the namespace has compiler rows */
void ck_cap_schemes(ck_ctx *c); /* the injected pair's schemes */
const ek_type *ck_nominal(ck_ctx *c, const ek_symbol *sym, const ek_type **args,
                          size_t argc);
const ek_type *ck_nominal_fresh(ck_ctx *c, const ek_symbol *sym);
const ek_type *ck_nominal_fresh_n(ck_ctx *c, const ek_symbol *sym, size_t n);
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

/* True when more than one module of the closure declares a type of
   this name: a printable name then carries the declaring module. */
int ck_shared_name(ck_ctx *c, const char *name);

/* True for a member name the checker types by derivation rather than by
   a declaration: the equality pair, the ordering family, the display
   pair, with, and the JSON pair. */
int ck_service_name(const char *name);

/* membersvc.c: the derived services of a receiver's namespace. Each
   returns the service's arrow when it derives over the receiver, and
   NULL when the name is not the service's or the miss is diagnosed;
   ck_service_receiver is the receiver a service is written at, and
   ck_no_member reports a name no station answered. */
const ek_type *ck_derive_eq(ck_ctx *c, const ek_node *at,
                            const ek_type *r, const char *name,
                            ek_reading *rd, const ek_type *bool_t);
const ek_type *ck_derive_lt(ck_ctx *c, const ek_node *at,
                            const ek_type *r, const char *name,
                            ek_reading *rd, const ek_type *bool_t);
const ek_type *ck_derive_display(ck_ctx *c, const ek_type *r,
                                 const char *name, ek_reading *rd);
const ek_type *ck_derive_panic(ck_ctx *c, const ek_type *r,
                               const char *name, ek_reading *rd);
const ek_type *ck_derive_json(ck_ctx *c, const ek_node *at,
                              const ek_type *r, const char *name,
                              ek_reading *rd);
const ek_type *ck_derive_with(ck_ctx *c, const ek_node *at,
                              const ek_type *r, const char *name,
                              ek_reading *rd);
/* convert.c: the conversion member of a newtype receiver — the payload
   type's own name, answering the pure arrow from the newtype to the
   payload. */
const ek_type *ck_derive_convert(ck_ctx *c, const ek_type *r,
                                 const char *name, ek_reading *rd);
const ek_type *ck_service_receiver(ck_ctx *c, const ek_type *r);

/* reqs.c: the service obligations of a checked body. A service that
   can halt, used on a type variable, is recorded and decided at the
   type its instantiation gives, so a checked body cannot lower a
   service the concrete type does not have. */
int ck_service_halts(const char *name);
void ck_req_note(ck_ctx *c, const ek_node *at, const ek_type *r,
                 const char *service);
void ck_reqs_check(ck_ctx *c);
const ek_type *ck_no_member(ck_ctx *c, const ek_node *at,
                            const ek_type *r, const char *name,
                            const ek_symbol *mod);
void ck_row(ck_ctx *c, const ek_node *n, const ek_type *t, ek_reading r);
void ck_error(ck_ctx *c, const ek_node *n, const char *code, const char *fmt,
              ...);
void ck_warn(ck_ctx *c, const ek_node *n, const char *code, const char *fmt,
             ...);
const char *ck_file(const ck_ctx *c); /* the current module's file */

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

/* elab.c: annotations. repr.c: the anonymous representation of a
   shaped nominal type. decl.c: declared types, their binder scope,
   and the phantom rule. entry.c: entry rules. */
const ek_type *ck_elaborate(ck_ctx *c, const ek_node *t, ck_tvars *tv);
void ck_tv_add(ck_ctx *c, ck_tvars *tv, const char *name, const ek_type *t,
               const ek_node *first);
void ck_binder_scope(ck_ctx *c, const ck_decl *d, ck_tvars *tv);
const ek_type *ck_decl_repr(ck_ctx *c, const ek_type *nom);
const ek_type *ck_type_of(ck_ctx *c, const ek_symbol *sym);
ck_scheme *ck_scheme_of(ck_ctx *c, const ek_symbol *sym);
const ck_decl *ck_decl_of(ck_ctx *c, const ek_symbol *sym);
const ck_ctor *ck_ctor_of(ck_ctx *c, const ek_symbol *sym);
void ck_prebind(ck_ctx *c); /* unannotated top-level rows */
void ck_decls(ck_ctx *c);   /* the current module's type decls */
void ck_entry_rules(ck_ctx *c); /* the `main` rules over the closure */

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
const ek_type *ck_ctor_head(ck_ctx *c, const ek_node *n);

/* literal.c: literal placement and composite literals. */
int ck_is_numeric(const ek_type *t);      /* Int, UInt, Float */
int ck_is_machine_word(const ek_type *t); /* U8..I64 */
int ck_is_word_name(const char *n);       /* the machine-word spellings */
int ck_is_std(const ek_type *t, const char *name);
const ek_type *ck_std_type(ck_ctx *c, const char *name);
const ek_symbol *ck_std_symbol(ck_ctx *c, const char *name);
const ek_type *ck_place(ck_ctx *c, const ek_node *lit, const ek_type *expected,
                        int neg);
const ek_type *ck_composite(ck_ctx *c, const ek_node *e,
                            const ek_type *expected, int allow_partial);
/* recordlit.c: the record family of composite literals. */
const ek_type *ck_record_lit(ck_ctx *c, const ek_node *e, const ek_type *rec,
                             int allow_partial);
void ck_item_value(ck_ctx *c, const ek_node *val, const ek_type *expected,
                   int allow_partial);
void ck_pending_literal(ck_ctx *c, const ek_node *e);
const ek_ck_row *ck_row_of(const ck_ctx *c, const ek_node *n);
void finish_rows(ck_ctx *c); /* rows.c: sort, snapshot, world bits */

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
int ck_body_silent(ck_ctx *c, const ek_node *e); /* subtree holds a
                                   reported site that claims no bit */
ek_bit ck_call_bit_of(ck_ctx *c, const ek_type *f, const char *member,
                      const ek_type *recv, const ek_node *const *args,
                      const ek_type *const *types, size_t n, size_t off);
int ck_type_opens(const ek_type *t); /* contains an open arrow */
int ck_signature_opens(const ek_type *f); /* own arrow or a feeder */
void ck_fill_bits(ck_ctx *c); /* the final row pass (check.c) */

/* reconcile.c: the no-eq rule and the declared-bit binding rules. */
int ck_is_capability(const ek_type *t); /* the nine capability types */
int ck_is_handle(const ek_type *t); /* File, Scope, Task: opaque
                                        handles */
int ck_no_eq(ck_ctx *c, const ek_type *t); /* the eq derivation halts
                                              here */
void ck_no_eq_error(ck_ctx *c, const ek_node *at, const ek_type *t);
void ck_key_domain_error(ck_ctx *c, const ek_node *at, const ek_type *t);
void ck_dict_key_name(ck_ctx *c, const ek_node *key); /* an unbound
    bare-name dictionary key is the 7.4.2 miss */
int ck_no_json(ck_ctx *c, const ek_type *t); /* the JSON mapping halts
                                                here */
void ck_json_shape_error(ck_ctx *c, const ek_node *at, const ek_type *t);
void ck_open_positions(ck_ctx *c, const ek_node *t, int allow);
int ck_annotation_bits(ck_ctx *c, const ek_node *ann, const ek_type *bound);
ek_bit ck_own_bit(const ek_type *bound);
void ck_reconcile(ck_ctx *c, const ek_node *at, const ek_type *bound,
                  const ek_node *val);

/* pattern.c: pattern typing. patcons.c: constructor patterns.
   patbind.c: or-patterns and the binding-shape judgments.
   irrefutable.c: the value binding's irrefutability judgment. */
int ck_invert_pattern(const ek_node *pat);
const ek_type *ck_invert_binding(ck_ctx *c, const ek_node *m,
                                  const ek_node *pat, int top);
const ek_type *ck_invert_block(ck_ctx *c, const ek_node *m,
                               const ek_node *block, size_t start,
                               const ek_type *expected);
const ek_type *ck_pattern(ck_ctx *c, const ek_node *p, const ek_type *scrut);
const ek_type *cons_pattern(ck_ctx *c, const ek_node *p,
                            const ek_type *scrut);
const ek_symbol *ck_cons_select(ck_ctx *c, const ek_node *p,
                                const ek_type *scrut);
const ek_type *ck_or_pattern(ck_ctx *c, const ek_node *p, const ek_type *scrut);
void ck_arm_binds_together(ck_ctx *c, const ek_node *arm); /* a name two
                                        of the arm's patterns bind conflicts */
int ck_irrefutable(ck_ctx *c, const ek_node *p, const ek_type *t);
/* 0 = refutable in a value binding */

/* exhaust.c: coverage of one scrutinee. exhaust_prod.c: the
   multi-scrutinee product and the entry. */
int ck_covers(ck_ctx *c, const ek_type *t, const ek_node *const *pats,
              size_t n);
size_t ck_pat_flatten(ck_ctx *c, const ek_node *p, const ek_node **out,
                      size_t n, size_t cap);
const ek_symbol *ck_pat_head(ck_ctx *c, const ek_node *p);
int ck_pat_catchall(const ek_node *p);
int ex_any_catchall(const ek_node *const *alts, size_t n);
void ck_exhaust(ck_ctx *c, const ek_node *cs, const ek_type **scrut,
                size_t nscrut, const ek_node *const *arms, const int *contrib,
                size_t narms);

/* member.c: receiver-station selection and the link entries. link.c:
   the reading of one member link at its receiver's type. call.c:
   calls, lambdas, constructor application. */
/* call.c: the shared call machinery. callee_expr types the callee of
   an unknown-head call; call_fn checks one call against a function
   type, receiver first. */
const ek_type *callee_expr(ck_ctx *c, const ek_node *callee,
                           const ek_node *call);
const ek_type *callee_symbol_type(ck_ctx *c, const ek_symbol *sym,
                                  const ek_node *call);
const ek_type *call_fn(ck_ctx *c, const ek_node *e, const ek_type *fntype,
                       const char *member, const ek_type *recv,
                       const ek_type *bit_recv, const ek_nlist *args,
                       int synthesize, int implicit);
const ek_type *arg_conform(ck_ctx *c, const ek_node *arg,
                           const ek_type *ptype);

/* callchain.c: the receiver-chain dispatch. type_head reads a
   PascalCase callee head as a type namespace; the member, service,
   and constructor walks type one link. */
int type_head(ck_ctx *c, const ek_node *recv, const ek_type **out);
const ek_type *call_member(ck_ctx *c, const ek_node *e,
                           const ek_node *callee, int implicit);
const ek_type *call_service(ck_ctx *c, const ek_node *e,
                            const ek_node *callee, int implicit);
const ek_type *call_ctor(ck_ctx *c, const ek_node *e, const ek_node *callee,
                         const ek_ref *ref, const ek_type *expected,
                         int implicit);

const ek_type *ck_call(ck_ctx *c, const ek_node *e, const ek_type *expected);
const ek_type *ck_field(ck_ctx *c, const ek_node *e);
const ek_type *ck_link(ck_ctx *c, const ek_node *link, const ek_type *recv,
                       const char *name, ek_reading *rd);
const ek_symbol *ns_scan(ck_ctx *c, const char *want, const char *owner,
                         const char *name); /* a declared namespace member
   in a module's surface, by owner and spelling */
const ek_type *ck_link_mod(ck_ctx *c, const ek_node *link,
                           const ek_type *recv, const char *name,
                           const ek_symbol *mod, ek_reading *rd);
const ek_type *ck_field_of(ck_ctx *c, const ek_type *r, const char *name);
/* link.c: the reading of one member link, dispatched by the receiver's
   kind, and the record-field lookup its arms and ck_field_of share. */
const ek_type *link_type(ck_ctx *c, const ek_node *at, const ek_type *recv,
                         const char *name, const ek_symbol *mod,
                         ek_reading *rd);
const ek_type *field_of(const ek_type *r, const char *name);
const ek_type *ck_lambda(ck_ctx *c, const ek_node *e, const ek_type *expected);
const ek_type *ck_apply(ck_ctx *c, const ek_node *e, const ek_type *expected);
const ek_type *ctor_apply(ck_ctx *c, const ek_node *e, const ck_ctor *ct,
                          const ek_type *expected, ek_node *const *args,
                          size_t nargs);
int ck_ref_at(const ck_ctx *c, const ek_node *n, ek_sym_kind kind,
              const ek_ref **out);

/* extern.c: the compile-time extern and ctype checks over the current
   module's declarations. */
void ck_externs(ck_ctx *c);

/* instdepth.c: the instantiation graph and its instance set
   (Clause 8.7.3). An instantiation in a body records an edge; the
   final pass expands ground seeds until the set is exhausted or the
   depth bound is crossed. */
const ek_symbol *ck_decl_symbol(ck_ctx *c, const ek_node *m);
const ek_type *ck_instantiate_use(ck_ctx *c, const char *name,
                                  const ck_scheme *s);
void ck_inst_depth(ck_ctx *c);

/* bind.c: the binding forms and ck_stmt. declcheck.c: the annotated
   bindings and the declared members; a namespace member or a test
   body runs under a fresh instantiation state. */
ck_scheme *ck_mono(ck_ctx *c, const ek_type *t);
const ek_type *ck_stmt(ck_ctx *c, const ek_node *m, int top);
void annotated_fn_body(ck_ctx *c, const ek_node *ann, const ek_node *val,
                       const ek_type *bound);
const ek_type *annotated_binding(ck_ctx *c, const ek_node *m,
                                 const ek_node *val, ck_scheme *gen,
                                 int top);
const ek_type *ck_annotated_decl(ck_ctx *c, const ek_node *m,
                                 const ek_node *ann, const ek_node *body,
                                 int binding);
const ek_type *ck_namespace_member(ck_ctx *c, const ek_node *m);
void ck_test_decl(ck_ctx *c, const ek_node *m);

#endif
