#ifndef EK_LOWER_INTERNAL_H
#define EK_LOWER_INTERNAL_H

#include <stdarg.h>
#include <stddef.h>

#include "check/check.h"
#include "lower/lower.h"
#include "resolve/resolve.h"
#include "support/arena.h"
#include "support/buf.h"
#include "support/diag.h"

/* The calling convention of the emitted functions and of every
   runtime entry they call: the caller keeps every argument value alive
   across the whole call, a parameter is borrowed -- the callee retains
   only what it stores -- and the caller's own frame releases its
   arguments' owning references when its scopes close. A call
   argument the caller's frame already owns therefore travels without
   a keep, and a parameter the body only reads stays borrowed. Any
   future indirect-call mechanism must honor the same invariant, or
   its arguments need explicit keeps again. */

/* One visible binding of an emitted body: the written name, its C
   spelling, and its kind. Owned bindings release at their scope's
   close; borrowed bindings are reads through another owner (the
   captured environment of a closure) and never release. A parameter
   the body only reads aliases its ABI slot: no keep is born, and the
   scope close releases nothing. */
typedef struct {
    const char *eko;
    const char *cname;
    int is_param;         /* borrowed: a read through a closure env */
    const char *abi;      /* the borrowed ABI slot of a parameter */
    int aliased;          /* the parameter local aliases its ABI slot */
} lower_local;

/* One C block of an emitted body: its bindings in declaration order,
   released at the block's end, the temps minted in it, and its
   enclosing block. */
typedef struct lower_scope {
    lower_local *locs;
    size_t nloc, cloc;
    const char **temps;
    size_t ntemp, ctemp;
    struct lower_scope *parent;
} lower_scope;

/* An expression result: the C text of an owned value and whether the
   text names a temp (movable by assignment) or is an inline
   expression. */
typedef struct {
    const char *text;
    int is_temp;
} lower_val;

/* The layout family of a generated struct: a named record layout, a
   positional tuple layout, or a one-slot constructor payload. */
typedef enum {
    GT_REC, GT_TUP, GT_CTOR
} lower_gshape;

/* One generated struct family: a declared type's record or tuple
   representation, one constructor of an algebraic type, or an
   anonymous record or tuple shape. Fields are ek_val in every family;
   the stem derives the struct, typeinfo, drop, and helper spellings
   (ekS_, ekT_, drop_, ekn_). */
typedef struct lower_gtype {
    const char *stem;      /* the shared name stem */
    const char *helper;    /* the construction helper's C name */
    const char **fnames;   /* record field names in layout order */
    size_t nf;
    lower_gshape shape;
    const ek_symbol *key_sym; /* registry identity: the type symbol of
                                 a representation, the constructor
                                 symbol of a constructor family */
    const char *key_str;   /* registry identity of an anonymous shape */
    const char *eq_name;   /* the stamped structural eq, NULL when the
                              family keys on identity */
    const char *hash_name; /* the stamped structural hash, NULL when
                              the family keys on identity */
    int used;              /* an emitted body references the layout */
    int constructed;       /* the unit constructs it: typeinfo, drop,
                              and helper join the typedef */
    int shared;            /* the runtime builds the values: typeinfo
                              and drop join the typedef without the
                              helper, and the typeinfo keeps external
                              linkage the runtime names */
} lower_gtype;

/* The runtime slot a std scalar occupies: the domain words widen into
   the Int, UInt, and Float slots. */
typedef enum {
    EKSC_OTHER, EKSC_INT, EKSC_UINT, EKSC_FLOAT, EKSC_CHAR,
    EKSC_BOOL, EKSC_STRING, EKSC_BYTES
} lower_scalar;

/* One variant of an algebraic receiver: its spelling, its constructor
   family's stem (the typeinfo the dispatch tests), and the applied
   payload type, NULL for a zero-payload variant. */
typedef struct {
    const char *name;
    const char *stem;
    const ek_type *payload;
} lower_variant;

static inline lower_val lower_val_unit(void)
{
    lower_val v;

    v.text = "ek_val_unit()";
    v.is_temp = 0;
    return v;
}

/* One deferred definition appended after the function that created it:
   a closure body still to lower, or the ready text of a trampoline.
   A closure body created inside a specialized function carries the
   instantiation that body emitted under, so the lambda's own rows
   substitute the same way. */
typedef struct lower_pending {
    const char *text;         /* ready definition text, or NULL */
    const ek_node **members;  /* the body members */
    size_t nmembers;
    const char *name;         /* the defining function's C name */
    const char **caps;        /* the captured names, in capture order */
    size_t ncap;
    const char **params;      /* the parameter names; NULL when unnamed */
    size_t nparams;
    const char *stem;         /* the environment family's stem */
    const char *file;         /* the owner's surface file */
    const char **env_names;   /* the owner instantiation's binders */
    const ek_type **env_args; /* the applied arguments, aligned */
    size_t nenv;
    struct lower_pending *next;
} lower_pending;

/* One reachability item: the function symbol and the instantiation
   its body emits under. A non-generic function takes the empty
   instantiation and keeps its plain name; a generic one's name spells
   the applied type arguments, and args carries them in binder order
   with fn, the applied function type the pairing pairs against. */
typedef struct {
    const char *name;       /* the emitted function's C name */
    const ek_symbol *sym;
    const ek_type **args;   /* the applied arguments; NULL when plain */
    size_t nargs;
    const ek_type *fn;      /* the applied function type, NULL when plain */
} lower_item;

/* One trampoline: the function it forwards to and its C name. */
typedef struct {
    const ek_symbol *sym;
    const char *name;
} lower_tramp;

/* One interned string literal of the unit: the cache variable that
   holds it and the escaped C literal the shim primes it with. */
typedef struct {
    const char *name;
    const char *lit;
} lower_strlit;

/* One test binding of a TESTS-mode unit: the declared name the
   dispatching shim matches against and the C name it calls. */
typedef struct {
    const char *name;
    const char *cname;
} lower_test;

/* The marshalling shape of one extern signature type, classified from
   the written annotation; the C namespace spellings live only there,
   so the shape is re-derived at the boundary, not read off the check
   row. */
typedef enum {
    MS_BOOL, MS_INT, MS_UINT, MS_FLOAT, MS_WORD, MS_STRING, MS_BYTES,
    MS_CTYPE, MS_CIN, MS_CIN_LIST, MS_PTR, MS_SIZE, MS_CLOSURE, MS_UNIT,
    MS_BAD
} lower_ms_kind;

typedef struct {
    lower_ms_kind kind;
    const char *word;       /* the machine word's spelling, MS_WORD */
    const ek_symbol *ctype; /* the declaration symbol, MS_CTYPE and
                               MS_CIN */
    const ek_node *fn;      /* the written arrow, MS_CLOSURE */
} lower_mshape;

/* One emitted prototype of an extern declaration: the symbol's
   prototype joins the unit once, at its first call site. */
typedef struct {
    const ek_symbol *sym;
    int bad; /* the signature has a shape without a lowering */
} lower_ext;

/* One callback adapter: the target-callable function and the context
   struct type of one extern signature's closure parameter. */
typedef struct {
    const ek_symbol *esym;
    size_t param;
    const char *fn;    /* the adapter's C name, NULL on a failed shape */
    const char *ctxty; /* the context struct's C name */
    int bad;           /* the callback signature has no lowering */
} lower_cbr;

/* One derived service emitted for an applied type: the structural key
   that shares one body among equal spellings, the C name, and the
   family the body renders. */
typedef struct {
    const char *key;
    const char *name;
    unsigned char kind;
} lower_dsvc;

/* The service families. */
typedef enum {
    DSV_EQ,        /* int(ek_val a, ek_val b): structural equality */
    DSV_ORD,       /* int(ek_val a, ek_val b): -1, 0, or 1 */
    DSV_HASH,      /* unsigned long(ek_val v): the structural hash */
    DSV_SHOW,      /* ek_obj *(ek_val v): the display channel */
    DSV_INSPECT,   /* ek_obj *(ek_val v): the structural channel */
    DSV_TOJSON,    /* ek_val(ek_val v): the Json value of v */
    DSV_FROMJSON   /* ek_val(ek_val j): Result of the decode */
} lower_dsvc_kind;

typedef struct lower_ctx lower_ctx;

/* The text emitter of one service body: writes into the body's own
   buffer and mints the body's temps. A body opens its buffer, emits,
   and closes it, so a child requested mid-body joins the unit whole
   instead of splitting the parent's text. */
typedef struct {
    lower_ctx *c;
    unsigned tmp;
    ek_buf *buf;
} lower_demit;

/* Lowering state. Function text accumulates in body; the entry point
   assembles the unit around it. work/seen drive reachability; file is
   the source file of the function being emitted; depth is the
   indentation level of the statement being emitted; in_tail marks a
   tail position of the function being emitted. */
struct lower_ctx {
    ek_resolve_result resolved;
    ek_check_result checked;
    ek_arena *arena;
    ek_diag *diag;
    ek_buf body;
    const char **sigs;
    size_t nsig, csig;
    lower_item *work; /* the reachable (symbol, instantiation) pairs */
    size_t nwork, cwork, head;
    lower_item *seen; /* the items already queued, keyed by name */
    size_t nseen, cseen;
    /* The instantiation the body being emitted specializes: the
       function's binder names and the applied arguments, aligned.
       Empty for a non-specialized body. */
    const char **env_names;
    const ek_type **env_args;
    size_t nenv, cenv_cap;
    const ek_symbol *entry;
    const char *entry_mangled;
    const char *file;
    /* The source position the last #line directive named: a statement
       marks its position only when the emission moved. */
    const char *line_file;
    int line_no;
    unsigned depth;
    unsigned temp; /* per-function temp counter */
    int in_tail;   /* the position being emitted is a tail of the
                      function being emitted */
    int framed;    /* the function being emitted pushed a frame */
    /* The self tail-call transform of the function being emitted: its
       own symbol (NULL inside a closure body), its owned parameter
       locals, the borrow-free scope the release walk stops at, and the
       retry label when the body has a tail self-call. */
    const ek_symbol *tce_self;
    const lower_scope *tce_params;
    const char *tce_label;
    /* The reference temps a case cascade abandons at a retry jump:
       scrutinee temps and arm walk temps, live at the point reached. */
    const char **clean;
    size_t nclean, cclean;
    /* Deferred closure bodies and trampolines, appended after the
       function that created them. */
    lower_pending *pend, *pend_tail;
    unsigned lambda;   /* closure bodies emitted so far */
    unsigned anon_env; /* environment struct families so far */
    lower_tramp *tramps; /* the trampolines, in creation order */
    size_t ntramp, ctramp;
    lower_tramp *idents; /* the injected pair identities, in creation
                            order */
    size_t nident, cident;
    ek_buf cext;         /* the ctype C structs and extern prototypes,
                            joined after the preamble */
    lower_ext *exts;     /* the emitted prototypes, in first-call order */
    size_t next_r, cext_r;
    lower_cbr *cbrs;     /* the callback adapters, in creation order */
    size_t ncbr, ccbr;
    unsigned cbr;        /* the callback adapters minted so far */
    const ek_symbol **ctypedefs; /* the emitted ctype C structs */
    size_t nct, cct;
    size_t fn_count;
    lower_gtype *gtypes; /* the generated struct families, in first-use
                            order */
    size_t ngt, cgt;
    lower_dsvc *dsvcs; /* the derived services, in first-use order */
    size_t ndsvc, cdsvc;
    ek_buf dsvc;       /* the derived service bodies, joined after the
                          functions */
    lower_strlit *strlits; /* the interned string literals, in
                              first-use order */
    size_t nslit, cslit;
    ek_buf strdecls;   /* the cache variable declarations, joined after
                          the preamble */
    unsigned anon_rec; /* the anonymous record families so far */
    int uses_list; /* some body folds a list literal */
    int uses_string_h; /* some body calls a <string.h> function */
    int hash_mix;  /* the hash bodies' mixing helper is emitted */
    int has_main;
    ek_lower_mode mode;  /* what the unit is for: the roots and the
                            entry shim */
    const char *shim[9]; /* the entry shim's bundle constructors, by
                            position (the manifest's capability order) */
    size_t nshim;
    lower_test *tests; /* the test bindings, in emission order */
    size_t ntests, ctests;
    /* The expected type one subtree lowers under: a constructor
       application whose payload the check row left opaque reads the
       constructor's own payload type, and the composite items under it
       read their field or element types the same way. */
    const ek_node *exp_node;
    const ek_type *exp_type;
    int oom;
};

/* demit.c: the writer of one service body. */
void lower_d_begin(lower_ctx *c, lower_demit *d, ek_buf *b);
void lower_d_end(lower_ctx *c, ek_buf *b);
void lower_d_printf(lower_demit *d, const char *fmt, ...);
const char *lower_d_temp(lower_demit *d);
void lower_d_concat_lit(lower_demit *d, const char *indent, const char *acc,
                        const char *lit);
void lower_d_concat_obj(lower_demit *d, const char *indent, const char *acc,
                        const char *part);
const char *lower_d_cstr(lower_ctx *c, const char *s);

/* derived.c: the derived services' classification, and the shape and
   variant readers that feed the body emitters. */
lower_scalar lower_scalar_kind(const ek_type *t);
int lower_is_range(const ek_type *t);
int lower_is_handle(const ek_type *t);
int lower_is_set(const ek_type *t);
int lower_is_vector(const ek_type *t);
int lower_shape_of(lower_ctx *c, const ek_node *at, const ek_type *t,
                   const char ***fnames, const ek_type ***ftypes, size_t *nf,
                   lower_gtype **g);
int lower_variants_of(lower_ctx *c, const ek_node *at, const ek_type *t,
                      lower_variant **out, size_t *nv, int construct);
const char *lower_std_ctor(lower_ctx *c, const ek_node *at,
                           const char *owner, const char *name);

/* typewalk.c: the applied type of a type-syntax node under a
   declaration's instantiation. */
const ek_type *lower_applied_type(lower_ctx *c, const ek_node *tnode,
                                  const ek_node *decl, const ek_type *owner);
const ek_symbol *lower_type_symbol(lower_ctx *c, const char *mod,
                                   const char *name);

/* dsvcreg.c: the result type a derived-service call returns, when the
   site's open receiver substitutes concrete under the instantiation
   being emitted -- the type the specialized body reads the call at.
   NULL when the site is not such a call. */
const ek_type *lower_open_call_result(lower_ctx *c, const ek_node *e);

/* call.c: the declared member of one receiver type's namespace, found
   the way the member-call path finds it; NULL when no declaration
   owns the name. */
const ek_symbol *lower_member_symbol(lower_ctx *c, const ek_type *recv,
                                     const char *name);

/* derivedjson.c, derivedfrom.c: the JSON emitters and their shared
   spellings. OK_INFO is the typeinfo test of the std Result's Ok. */
#define LOWER_OK_INFO "&ekT_std$Result$Ok" 
const char *lower_json_ctor(lower_ctx *c, const char *variant);
const char *lower_result_ctor(lower_ctx *c, const char *name);
void lower_json_err_lit(lower_demit *d, const char *indent,
                        const char *detail);
void lower_json_err_obj(lower_demit *d, const char *indent, const char *obj);
const char *lower_json_slot(lower_ctx *c, const char *variant,
                            const char *v);
const char *lower_fromj_of(lower_ctx *c, const ek_type *t);
void lower_json_ok_emit(lower_demit *d, const char *indent, const char *expr);
void lower_val_text_is(lower_demit *d, const char *k, const char *raw);
void lower_obj_text_is(lower_demit *d, const char *obj, const char *raw);
void lower_from_tuple_emit(lower_ctx *c, lower_demit *d, lower_gtype *g,
                           const ek_type **ftypes, size_t nf);
void lower_from_record_emit(lower_ctx *c, lower_demit *d, const char *name,
                            const char **fnames, const ek_type **ftypes,
                            size_t nf, lower_gtype *g);

/* derivedadt.c: the externally tagged decode of an algebraic type. */
void lower_from_adt_emit(lower_ctx *c, lower_demit *d, lower_variant *vs,
                         size_t nv);

/* derivedshow.c, derivedjson.c, derivedfrom.c: the walkers the
   container bodies build on -- the brace form over one cons spine,
   the recursive cell walk behind an Arr, and its decode. */
void lower_seq_show_emit(lower_ctx *c, lower_demit *d, const ek_type *elemt,
                         const char *src, int owned);
void lower_to_list_aux(lower_ctx *c, const char *aux, const ek_type *elemt);
void lower_from_list_aux(lower_ctx *c, const char *aux, const ek_type *elemt);

/* derivedvec.c: the vector's derived-service bodies, one per family
   the registry mints over the placeholder. */
void lower_vec_show_emit(lower_ctx *c, lower_demit *d, const ek_type *t);
void lower_vec_eq_emit(lower_ctx *c, lower_demit *d, const ek_type *t);
void lower_vec_hash_emit(lower_ctx *c, lower_demit *d, const ek_type *t);
void lower_vec_tojson_emit(lower_ctx *c, lower_demit *d, const char *name,
                           const ek_type *t);
void lower_vec_fromjson_emit(lower_ctx *c, lower_demit *d, const char *name,
                             const ek_type *t);

/* derivedeq.c, derivedord.c, derivedhash.c, derivedshow.c,
   derivedjson.c: the body emitters. */
void lower_dsvc_eq_emit(lower_ctx *c, const char *name, const ek_type *t);
void lower_dsvc_ord_emit(lower_ctx *c, const char *name, const ek_type *t);
void lower_dsvc_hash_emit(lower_ctx *c, const char *name, const ek_type *t);
void lower_dsvc_show_emit(lower_ctx *c, const char *name, const ek_type *t,
                          int inspect);
void lower_dsvc_json_emit(lower_ctx *c, const char *name, const ek_type *t,
                          int from);
void lower_dsvc_from_emit(lower_ctx *c, const char *name, const ek_type *t);

/* dsvcstamp.c: the key-domain stamping. lower_key_pair names the
   hook pair a construction site passes the runtime over the key
   domain -- the stamped family services, the minted services of a
   container standing as the domain, or the runtime's default pair for
   the scalar and text slots -- and answers nonzero when the domain
   keys at all, zero for the domains whose derivation halts (functions,
   capabilities). lower_key_hooks stamps that pair onto the families
   that carry the domain's values, lazily at the dict and set sites
   that build over it. */
int lower_key_pair(lower_ctx *c, const ek_type *t, const char **keq,
                   const char **khash);
int lower_key_hooks(lower_ctx *c, const ek_type *t);

/* The form of a derived call: the receiver is the member link's node,
   the free call's first argument, or absent at a type head. */
typedef enum {
    CF_MEMBER, CF_FREE, CF_QUALIFIED
} lower_cmp_form;

/* dsvcspell.c: the applied-type spelling the service names and the
   specialized function names share. */
void spell_into(lower_ctx *c, ek_buf *b, const ek_type *t);
const char *lower_spell_apply(lower_ctx *c, const ek_type *const *args,
                              size_t n);

/* derived.c: the service registry and the call-site routing. */
const char *lower_dsvc_request(lower_ctx *c, const ek_node *at,
                               lower_dsvc_kind kind, const ek_type *t);
lower_val lower_derived_call(lower_ctx *c, lower_scope *scope,
                             const ek_node *e, const ek_node *callee,
                             const ek_ck_row *frow, lower_cmp_form form);
lower_val lower_derived_with(lower_ctx *c, lower_scope *scope,
                             const ek_node *e, const ek_node *recv);
const char *lower_derived_cmp(lower_ctx *c, const ek_node *e, ek_binop op);
/* derivedcmp.c: the service behind a comparison operator over the
   value layer's object slots (String, Bytes); NULL keeps the runtime
   entry. */

/* derivedcall.c: the service spelling behind a member name, -1 when
   the name is no service. */
int lower_service_kind(const char *name, lower_dsvc_kind *out);

/* lower.c: shared helpers. */
void lower_emitf(lower_ctx *c, const char *fmt, ...);
void lower_pad(lower_ctx *c);
const char *lower_str(lower_ctx *c, const char *fmt, ...);
const char *lower_take(lower_ctx *c, ek_buf *b);

/* typeemit.c: the C spelling of one generated struct field. */
const char *lower_field_c_name(lower_ctx *c, const lower_gtype *g,
                               size_t i);

/* bundle.c: the layout checks of one bundle record, after its typedef
   stands; a record the capability module does not name a bundle of
   checks nothing. */
void lower_bundle_checks(lower_ctx *c, ek_buf *u, const lower_gtype *g);
void *lower_grow(lower_ctx *c, void *data, size_t *cap, size_t need,
                 size_t width);
void lower_error(lower_ctx *c, const ek_node *at, const char *fmt, ...);
const char *surface_file(lower_ctx *c, const char *stem);
const ek_ck_row *lower_row(lower_ctx *c, const ek_node *n);
const ek_ref *lower_ref(lower_ctx *c, const ek_node *n);
void lower_enqueue(lower_ctx *c, const ek_symbol *sym);
void lower_enqueue_item(lower_ctx *c, const ek_symbol *sym,
                        const ek_type **args, size_t nargs,
                        const ek_type *fn);
void lower_clean_push(lower_ctx *c, const char *temp);
void lower_clean_pop(lower_ctx *c);
void lower_clean_release(lower_ctx *c, size_t base);
void lower_pending_add(lower_ctx *c, lower_pending *p);

/* unit.c: the translation-unit assembly. unit_preamble writes the
   runtime include the unit opens with; unit_assemble joins the front
   matter, the ctype structs and extern prototypes, the generated
   struct families, the forward declarations, the bodies, and the
   mode's shim, and copies the unit into the arena as the result. */
int unit_preamble(ek_buf *unit);
void unit_assemble(lower_ctx *c, ek_buf *unit, ek_lower_result *out,
                   ek_arena *arena);

/* value.c: the top-level value bindings. lower_is_value is true when
   the symbol is a module-level binding whose annotation is not a
   function type. The binding lowers to a lazily initialized static
   slot behind a getter; a reference in value position reads the
   getter, which pulls the binding into the unit. */
int lower_is_value(const ek_symbol *sym);
const char *lower_value_getter(lower_ctx *c, const ek_symbol *sym);
void lower_value_binding(lower_ctx *c, const lower_item *item);

/* typevar.c: the variable-occurrence queries over a type.
   lower_type_has_var is true when a tvar or an inference variable
   occurs anywhere; lower_type_has_ivar is the inference-variable-only
   form. */
int lower_type_has_var(const ek_type *t);
int lower_type_has_ivar(const ek_type *t);

/* subst.c: the instantiation substitution. Env build pairs the
   annotation's rigid variables with the applied function type of the
   call that keyed the emission, and every row-type read the emitter
   makes goes through lower_row_type so a specialized body sees its
   instantiation's types. */
int lower_env_build(lower_ctx *c, const ek_symbol *sym, const ek_type *fn);
size_t lower_inst_args(lower_ctx *c, const ek_symbol *sym,
                       const ek_type *fn, const ek_type ***out);
const ek_type *lower_type_in(lower_ctx *c, const ek_type *t);
const ek_type *lower_row_type(lower_ctx *c, const ek_node *n);
const ek_type *lower_std_nominal(lower_ctx *c, const char *name);

/* tce.c: the self tail-call transform. */
int lower_has_self_tail(lower_ctx *c, const ek_symbol *self,
                        const ek_node *body);
void lower_emit_self_tail(lower_ctx *c, lower_scope *scope,
                          const ek_node *e, const ek_ref *ref);

/* entryshim.c: the entry shim's bookkeeping. entry_manifest registers
   the capabilities a main's parameters declare and rejects the ones
   the shim cannot build; record_test registers one test binding for
   the dispatching shim, unless its name was recorded already. */
int entry_manifest(lower_ctx *c, const ek_node *decl, const ek_node *ann);
void record_test(lower_ctx *c, const ek_symbol *sym, const char *name);

/* name.c: the mangled function symbol, C spellings, string escapes. */
const char *lower_mangled(lower_ctx *c, const ek_symbol *sym);
const char *lower_inst_name(lower_ctx *c, const ek_symbol *sym,
                            const ek_type *const *args, size_t n);
const char *lower_temp(lower_ctx *c, lower_scope *scope);
const char *lower_c_name(lower_ctx *c, lower_scope *scope, const char *name);
void lower_string_literal(lower_ctx *c, ek_buf *b, const char *bytes,
                          size_t len);
const char *lower_str_cache(lower_ctx *c, const char *bytes, size_t len);

/* literal.c: literal placement by the check row's type. */
lower_val lower_int_lit(lower_ctx *c, const ek_node *e);
lower_val lower_float_lit(lower_ctx *c, const ek_node *e);
lower_val lower_char_lit(lower_ctx *c, const ek_node *e);
lower_val lower_string_expr(lower_ctx *c, lower_scope *scope,
                            const ek_node *e);

/* type.c: the generated struct layer. Registration happens while the
   bodies lower; the families themselves are emitted into the unit
   before the functions. */
lower_gtype *lower_decl_repr(lower_ctx *c, const ek_symbol *tsym);
lower_gtype *lower_gtype_of_stem(lower_ctx *c, const char *stem);
lower_gtype *lower_add_family(lower_ctx *c);
lower_gtype *lower_anon_record(lower_ctx *c, const ek_type *rec);
lower_gtype *lower_anon_tuple(lower_ctx *c, size_t n);
const ek_symbol *lower_pick_ctor(const ek_ref *ref, const ek_symbol *tsym);
lower_gtype *lower_find_family(const lower_ctx *c, const ek_symbol *sym,
                               const char *key);
lower_gtype *lower_register_ctor(lower_ctx *c, const ek_node *at,
                                 const ek_symbol *cs, const ek_node *decl,
                                 int construct);
lower_gtype *lower_ctor_family(lower_ctx *c, const ek_node *at,
                               const ek_ref *ref, const ek_symbol *tsym);
lower_gtype *lower_ctor_built(lower_ctx *c, const ek_node *at,
                              const ek_ref *ref, const ek_symbol *tsym);
lower_gtype *lower_ctor_of_symbol(lower_ctx *c, const ek_node *at,
                                  const ek_symbol *cs, int construct);
lower_gtype *lower_env_family(lower_ctx *c, const char **names, size_t n);
lower_gtype *lower_range_family(lower_ctx *c, lower_scalar slot);
const char *lower_field_slot(lower_ctx *c, const ek_node *at,
                             const ek_type *recv, const char *temp,
                             const char *name);
void lower_use_list(lower_ctx *c);
/* The declared record or tuple representation of a nominal as a value
   type, binders replaced; NULL when the declaration carries neither
   shape. */
const ek_type *lower_repr_type(lower_ctx *c, const ek_type *nom);
const ek_type *lower_record_field(const ek_type *rec, const char *name);
/* The payload type of the constructor a reference denotes, applied
   under the owner's instantiation; NULL for a zero-payload variant or
   a constructor the owner's declaration does not carry. */
const ek_type *lower_ctor_payload_type(lower_ctx *c, const ek_ref *ref,
                                       const ek_type *nom);
void lower_emit_types(lower_ctx *c, ek_buf *unit);

/* composite.c: constructors and composite literals. */
lower_val lower_ctor_value(lower_ctx *c, const ek_node *e, const ek_ref *ref);

/* composite.c: the item machinery behind every literal. The walkers
   are shared with the container and constructor constructions of
   construct.c. */
lower_val lower_item_under(lower_ctx *c, lower_scope *scope,
                           const ek_node *e, const ek_type *t);
const char **lower_item_temps(lower_ctx *c, lower_scope *scope,
                              const ek_node *e, const ek_type *t,
                              const ek_type *rec);
void lower_release_temps(lower_ctx *c, const char **argv, size_t n);
const ek_symbol *lower_row_nominal(lower_ctx *c, const ek_node *at);

/* construct.c: the composites born at a family border. */
lower_val lower_list_literal(lower_ctx *c, lower_scope *scope,
                             const ek_node *e);
lower_val lower_dict_literal(lower_ctx *c, lower_scope *scope,
                             const ek_node *e);
lower_val lower_ctor_apply(lower_ctx *c, lower_scope *scope,
                           const ek_node *e);
lower_val lower_composite(lower_ctx *c, lower_scope *scope,
                          const ek_node *e);

/* shapeadapt.c: the one-directional shape adaptation of one value: a
   nominal record flowing into an anonymous record expectation rebuilds
   field by field; any other pair comes back unchanged. */
lower_val lower_shape_adapt(lower_ctx *c, lower_scope *scope, lower_val v,
                            const ek_node *at, const ek_type *from,
                            const ek_type *to);

/* expr.c: expressions and construct naming for diagnostics. */
lower_val lower_expr(lower_ctx *c, lower_scope *scope, const ek_node *e);
lower_val lower_sub(lower_ctx *c, lower_scope *scope, const ek_node *e);

/* binop.c: the operator expressions -- numeric rows, derived
   comparisons, concatenation, the logical expansions, ranges, and
   negation. */
lower_val lower_binary_expr(lower_ctx *c, lower_scope *scope,
                            const ek_node *e);
lower_val lower_unary_expr(lower_ctx *c, lower_scope *scope,
                           const ek_node *e);
const char *lower_construct(const ek_node *n);

/* case.c: the case form and its arm cascade. */
lower_val lower_case(lower_ctx *c, lower_scope *scope, const ek_node *e);

/* pattern.c: one pattern's tests and bindings over a value slot. The
   walk buffers its test statements (unpadded lines), emits the literal
   hoists directly, and records the binding groups for the arm's
   pre-declarations and guarded assignments. */
typedef struct {
    const char *eko;
    const char *cname;
} lower_pat_decl;

typedef struct {
    const char *cname;
    const char *slot;
} lower_pat_item;

typedef struct {
    lower_pat_item *items;    /* one alternative's bindings */
    size_t nitem, citem;
    const char **conds;       /* the alternative's match conditions */
    size_t ncond, ccond;
} lower_pat_group;

typedef struct {
    lower_ctx *c;
    lower_scope *scope;     /* the arm scope: binding spellings mint here */
    const char *m;          /* the arm's match flag */
    const ek_type *col;     /* the column's effective type: the row's
                               own, or the recovered result of a derived
                               call the row left opaque */
    ek_buf stmts;           /* the test statements, unpadded lines */
    const char **parts;     /* the pending conjuncts, flushed in order */
    size_t npart, cpart;
    unsigned nflush;        /* the flushes so far */
    int failed;             /* some pattern had no lowering */
    lower_pat_decl *decls;  /* the pre-declared bindings, in order */
    size_t ndecl, cdecl;
    lower_pat_group *groups;
    size_t ngroup, cgroup;
    size_t cur;             /* the group receiving bindings */
} lower_pat;

void lower_pat_init(lower_ctx *c, lower_pat *pat, lower_scope *scope);
void lower_pat_column(lower_pat *pat, const ek_type *t);
const ek_type *lower_pat_type(lower_pat *pat, const ek_node *p);
void lower_pat_line(lower_pat *pat, const char *fmt, ...);
void lower_pat_cond(lower_pat *pat, const char *expr);
void lower_pat_new_group(lower_pat *pat);
void lower_pat_group_item(lower_pat *pat, const char *name,
                          const char *slot);
void lower_pat_flush(lower_pat *pat);
const char *lower_pat_is_cons(lower_ctx *c, const char *slot);
const char *lower_pat_slot_head(lower_ctx *c, const char *slot);
const char *lower_pat_slot_tail(lower_ctx *c, const char *slot);
void lower_pat_or(lower_pat *pat, const ek_node *p, const char *slot);
void lower_pat_brace(lower_pat *pat, const ek_node *p, const char *slot);
void lower_pat_step(lower_pat *pat, const ek_node *p, const char *slot);
int lower_pat_walk(lower_ctx *c, lower_pat *pat, const ek_node *p,
                   const char *slot);
void lower_pat_emit(lower_ctx *c, lower_pat *pat);
void lower_pat_assign(lower_ctx *c, lower_pat *pat);
void lower_pat_done(lower_ctx *c, lower_pat *pat);

/* call.c: the call forms. */
lower_val lower_call_expr(lower_ctx *c, lower_scope *scope,
                          const ek_node *e);
lower_val lower_call_extra(lower_ctx *c, lower_scope *scope,
                           const ek_node *e, const char *extra);

/* call.c: the borrowed-argument reading. lower_borrow_local names the
   frame-owned local the argument expression reads -- a name read of a
   binding the emitting frame owns, or the same reading through an
   injected identity call -- and NULL keeps the kept-temp shape.
   lower_arg_may_rebind is true when evaluating the expression can
   rebind a local of the emitting frame, so an argument borrowed
   before it would read the rebound value at the call. */
const char *lower_borrow_local(lower_ctx *c, lower_scope *scope,
                               const ek_node *e);
int lower_arg_may_rebind(const ek_node *e);

/* callargs.c: the call site's shape and argument placement. The form
   carries the receiver slot and the appended trailing argument; the
   injected identities of the capability module read as their
   argument; set_recv decides the receiver slot before the receiver
   lowers. */
typedef struct {
    int has_recv;
    lower_val recv;
    const char *recv_borrow;
    const char *extra;
} call_form;
lower_val panic_member_call(lower_ctx *c, lower_scope *scope,
                            const ek_node *e, const ek_node *recv);
const char *emit_arg_array(lower_ctx *c, lower_scope *scope,
                            const char **argv, size_t n);

int is_injected(const ek_symbol *sym);
const char *injected_identity(lower_ctx *c, const ek_symbol *sym);
void set_recv(lower_ctx *c, lower_scope *scope, call_form *member,
              const ek_node *recv, const ek_nlist *args, const ek_type *ft);
int args_rebind_from(const ek_nlist *args, size_t i);
const ek_type *fn_param(const ek_type *ft, size_t i);

/* callinv.c: the invocations over the filled argument slots -- the
   direct call of a known function, which a generic callee
   instantiates at the applied type, and the closure call of a
   function value. */
lower_val direct_call(lower_ctx *c, lower_scope *scope, const ek_node *e,
                      const ek_symbol *sym, const call_form *form);
/* callmember.c: the member-call readings -- the namespace scan, the
   open-receiver re-route, and the receiver, free-call, and
   current-module forms. lower_discard_call keeps the unsupported-call
   balance. */
lower_val lower_discard_call(lower_ctx *c, lower_scope *scope, const ek_node *e,
                       const call_form *form);
/* convert.c: the conversion member's payload read, over the receiver
   form and the type-namespace form alike. */
lower_val lower_convert_call(lower_ctx *c, lower_scope *scope,
                             const ek_node *e, const ek_node *callee,
                             const call_form *form, const ek_type *rt);
/* call.c: the value node beneath a chain's module and type segments,
   NULL when the chain heads at a module. */
const ek_node *module_path_recv(lower_ctx *c, const ek_node *recv);
lower_val member_call(lower_ctx *c, lower_scope *scope, const ek_node *e,
                      const ek_node *callee, const call_form *form);
lower_val c_ns_call(lower_ctx *c, lower_scope *scope, const ek_node *e,
                    const ek_node *callee, const call_form *form);
lower_val identity_call(lower_ctx *c, lower_scope *scope, const ek_node *e,
                        const ek_symbol *sym, const call_form *form);
lower_val free_member_call(lower_ctx *c, lower_scope *scope,
                           const ek_node *e, const ek_node *callee,
                           const call_form *form);

lower_val value_call(lower_ctx *c, lower_scope *scope, const ek_node *e,
                     lower_val callee, const char *extra);

/* borrow.c: the parameter-borrow analysis over one function body.
   lower_param_rebinds is true when the body rebinds the parameter of
   that written name -- the rebind releases the local's own reference
   before the new value moves in -- and lower_param_mentions is true
   when the body names the parameter anywhere, so the prologue knows
   whether the C parameter stays used. Both scan one shared child
   walk; the mentions scan enters lambdas, whose captures read the
   slot. */
int lower_param_rebinds(const ek_node *e, const char *name);
int lower_param_mentions(const ek_node *e, const char *name);

/* intrinsic.c: the compiler rows of the std namespaces routed to the
   runtime. The family is the receiver's std spelling; the call is
   handled when a table row matches, and left to the member path
   otherwise. */
const char *lower_intrinsic_family(const ek_type *t);
lower_val lower_intrinsic_call(lower_ctx *c, lower_scope *scope,
                               const ek_node *e, const ek_node *callee,
                               const char *family, int has_recv);

/* wrap.c: the std constructor wrapping of the status+out contracts.
   lower_wrap_helper registers the family of one std constructor and
   names its construction helper, with the payload arity in nf;
   lower_ctor_helper names a constructor of any module the same way. */
const char *lower_wrap_helper(lower_ctx *c, const ek_node *at,
                              const char *owner, const char *name, int *nf);
const char *lower_ctor_helper(lower_ctx *c, const ek_node *at,
                              const char *module, const char *owner,
                              const char *name, int *nf);
lower_val lower_wrap_option(lower_ctx *c, lower_scope *scope,
                            const ek_node *e, const char *sym,
                            const char *args, ek_out out);
lower_val lower_wrap_option_obj(lower_ctx *c, lower_scope *scope,
                                const ek_node *e, const char *sym,
                                const char *args);
lower_val lower_wrap_parse(lower_ctx *c, lower_scope *scope,
                           const ek_node *e, const char *sym,
                           const char *args, ek_out out, int radix);
lower_val lower_wrap_decode(lower_ctx *c, lower_scope *scope,
                            const ek_node *e, const char *sym,
                            const char *args);

/* capfam.c: the generated families the runtime builds over. A
   capability of the manifest registers the families its bundle pull
   names, and a pure derivation registers the families of the
   capability it constructs: the shim's bundle call and the
   derivation's constructor call pull the runtime's object files in
   through archive resolution, and the pulled members name the unit's
   identities. The registrations stamp the identities without the
   construction helpers: the runtime builds the values itself. */
void lower_cap_families_of(lower_ctx *c, const ek_node *at,
                           const char *cap);

/* capfam.c: every capability row at once, for the roots whose family
   needs no manifest bounds -- the test binaries. */
void lower_cap_families(lower_ctx *c, const ek_node *at);

/* capfam.c: the Json and ParseError constructor families the runtime's
   document reader builds values over, stamped as one set exactly when
   the parse row routes -- the rt references stay inside json.o, so a
   unit without the routing links without the identities. */
void lower_json_families(lower_ctx *c, const ek_node *at);

/* capfam.c: the time parse identities -- the reading and span
   constructors with the status and error sets -- stamped when the two
   time parse rows route. */
void lower_time_families(lower_ctx *c, const ek_node *at);

/* capfam.c: the seven Json variant shapes the derived decode bodies
   read, stamped as one set without the construction helpers. */
void lower_json_shapes(lower_ctx *c, const ek_node *at);

/* unwrap.c: the panicking observations of the Option and Result
   namespaces, the compiler rows `unwrap` and `expect`. A NULL result
   hands the decision back. */
lower_val lower_unwrap_call(lower_ctx *c, lower_scope *scope,
                            const ek_node *e, const ek_node *callee,
                            const char *family, const char *member,
                            int has_recv);

/* capwrap.c: the capability channel's wrapping. The runtime's status
   contracts over the file family wrap into the std Result and the
   capability module's IoError; ok picks the success arm's shape. */
typedef enum {
    LOWER_IO_OK_REF,  /* an owned object payload */
    LOWER_IO_OK_UNIT, /* the unit payload */
    LOWER_IO_OK_OPT   /* an owned object or end of input: Some or None */
} lower_io_ok;

lower_val lower_wrap_io(lower_ctx *c, lower_scope *scope, const ek_node *e,
                        const char *sym, const char *args, lower_io_ok ok);
lower_val lower_wrap_exit(lower_ctx *c, lower_scope *scope, const char *sym,
                          const char *args);

/* capwrap.c: the compiler-provided rows of the capability channel --
   the eight File members, the process exit, and the two advancing
   derivations. A NULL result hands the decision back. */
lower_val lower_cap_call(lower_ctx *c, lower_scope *scope,
                         const ek_node *e, const ek_node *callee,
                         const char *family, int has_recv);

/* capture.c: the capture set of a closure body. */
typedef struct {
    lower_ctx *c;
    lower_scope *at;      /* the creation site: the names it sees */
    const char **names;
    size_t n, cap;
} lower_reads;

void lower_reads_walk(lower_reads *r, const ek_node *e);
void lower_pat_param(lower_ctx *c, const ek_node *pat, const char ***names,
                     size_t *n, size_t *cap);
void lower_lambda_param_names(lower_ctx *c, const ek_node *params,
                              const char ***names, size_t *n, size_t *cap);
void lower_body_members(lower_ctx *c, const ek_node *body,
                        const ek_node ***members, size_t *nmembers);
void lower_push_sig(lower_ctx *c, const char *sig);
const char *lower_emit_closure_new(lower_ctx *c, lower_scope *scope,
                                   const ek_node *at, lower_gtype *g,
                                   const char **names, size_t n,
                                   const char *fnname);

/* lambda.c: closures, captures, trampolines, and the `<-` chains. */
lower_val lower_lambda(lower_ctx *c, lower_scope *scope, const ek_node *e);
lower_val lower_invert(lower_ctx *c, lower_scope *scope, const ek_node *m,
                       const ek_node **members, size_t nmembers);
const char *lower_trampoline(lower_ctx *c, const ek_symbol *sym);
void lower_flush_pending(lower_ctx *c);/* body.c: functions, statement members, and the if form. */
void lower_function(lower_ctx *c, const lower_item *item);
void lower_if(lower_ctx *c, lower_scope *scope, const ek_node *e,
              lower_val *out);
void lower_scope_add(lower_ctx *c, lower_scope *s, const char *eko,
                     const char *cname, int is_param, const char *abi);
const char *lower_as_temp(lower_ctx *c, lower_scope *scope, lower_val *v);
void lower_release_scope(lower_ctx *c, lower_scope *s);
lower_val lower_block_value(lower_ctx *c, lower_scope *scope,
                            const ek_node *body);
lower_val lower_members(lower_ctx *c, lower_scope *scope,
                        const ek_node **kids, size_t n);
const lower_local *scope_find_local(lower_scope *scope, const char *name);

/* destructure.c: the irrefutable destructuring of an initial binding.
   1 when the pattern lowered, 0 to keep the general diagnostic. */
int lower_binding_destructure(lower_ctx *c, lower_scope *scope,
                              const ek_node *m, const ek_node *pat);
int lower_rebind_destructure(lower_ctx *c, lower_scope *scope,
                             const ek_node *m, const ek_node *pat);
int lower_value_destructure(lower_ctx *c, lower_scope *scope,
                            const ek_node *m, const ek_node *pat,
                            const char *name);

/* extctype.c: the frozen C struct of a ctype and the walks between
   its two shapes. lower_ctype_of_node names the declaration a written
   annotation node denotes when it names a ctype, NULL otherwise.
   lower_ctype_shape registers the value family and the frozen C
   struct of a ctype declaration, returning the family or NULL.
   lower_ctype_pack_expr renders the compound literal carrying the ek
   value expression across the boundary by value; lower_ctype_unpack
   renders the ek_val builder expression over the C struct expression
   src. */
const ek_symbol *lower_ctype_of_node(lower_ctx *c, const ek_node *tn);
lower_gtype *lower_ctype_shape(lower_ctx *c, const ek_symbol *tsym);
const char *lower_ctype_pack_expr(lower_ctx *c, const ek_symbol *tsym,
                                  const char *src);
const char *lower_ctype_unpack(lower_ctx *c, const ek_symbol *tsym,
                               const char *src);

/* extshape.c: the shape side of the whitelist. lower_word_ctype names
   the exact-width C type of a machine word's spelling, NULL outside
   the word set. lower_ms_of classifies one written annotation node; a
   NULL node is the elided unit result. lower_scalar_out_expr names
   the C expression reading one scalar shape out of a value temp and
   rebuilding one back from a C value, and lower_ms_param_type the C
   type of one shape in a parameter position. The two callback rows name the parameter and result C
   types in the direction each flows, NULL outside the set. */
const char *lower_cbr_param_type(lower_ctx *c, const lower_mshape *s);
const char *lower_cbr_ret_type(lower_ctx *c, const lower_mshape *s);
const char *lower_word_ctype(const char *name);
const char *lower_word_out(lower_ctx *c, const char *word, const char *v);
const char *lower_word_in(lower_ctx *c, const char *word, const char *r);
lower_mshape lower_ms_of(lower_ctx *c, const ek_node *tn);
const char *lower_scalar_out_expr(lower_ctx *c, const lower_mshape *s,
                                  const char *v);
const char *lower_scalar_in_expr(lower_ctx *c, const lower_mshape *s,
                                 const char *r);
const char *lower_ms_param_type(lower_ctx *c, const lower_mshape *s);

/* extproto.c: the registry entry of one extern declaration, created
   at its first call site and carrying its prototype and its bad flag;
   NULL on an allocation failure. */
lower_ext *extern_entry(lower_ctx *c, const ek_symbol *sym);

/* extern.c: the native call path. lower_extern_call lowers one call
   whose reference denotes an extern declaration: the prototype joins
   the unit at the first call site, every argument marshals per the
   whitelist, and the result marshals back. extra is the appended
   callback of a chain, which an extern call does not take. */
lower_val lower_extern_call(lower_ctx *c, lower_scope *scope,
                            const ek_node *e, const ek_symbol *sym,
                            const char *extra);

/* extcback.c: the callback adapters. lower_cbr_ensure returns the
   adapter and context-struct names of one extern signature's closure
   parameter, emitting them once; 0 means the callback signature has
   no lowering and the diagnostic stands. */
int lower_cbr_ensure(lower_ctx *c, const ek_symbol *esym, size_t param,
                     const char **fn, const char **ctxty);

#endif
