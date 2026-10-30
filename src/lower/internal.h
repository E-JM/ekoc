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

/* One visible binding of an emitted body: the written name, its C
   spelling, and its kind. Owned bindings release at their scope's
   close; borrowed bindings are reads through another owner (the
   captured environment of a closure) and never release. */
typedef struct {
    const char *eko;
    const char *cname;
    int is_param;         /* borrowed: a read through a closure env */
    const char *abi;      /* the borrowed ABI slot of a parameter */
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
   a closure body still to lower, or the ready text of a trampoline. */
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
    struct lower_pending *next;
} lower_pending;

/* One trampoline: the function it forwards to and its C name. */
typedef struct {
    const ek_symbol *sym;
    const char *name;
} lower_tramp;

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
    const ek_symbol **work;
    size_t nwork, cwork, head;
    const ek_symbol **seen;
    size_t nseen, cseen;
    const ek_symbol *entry;
    const char *entry_mangled;
    const char *file;
    unsigned depth;
    unsigned temp; /* per-function temp counter */
    int in_tail;   /* the position being emitted is a tail of the
                      function being emitted */
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
    size_t fn_count;
    lower_gtype *gtypes; /* the generated struct families, in first-use
                            order */
    size_t ngt, cgt;
    lower_dsvc *dsvcs; /* the derived services, in first-use order */
    size_t ndsvc, cdsvc;
    ek_buf dsvc;       /* the derived service bodies, joined after the
                          functions */
    unsigned anon_rec; /* the anonymous record families so far */
    int uses_list; /* some body folds a list literal */
    int has_main;
    const char *shim[9]; /* the entry shim's bundle constructors, by
                            position (the manifest's capability order) */
    size_t nshim;
    int oom;
};

/* derived.c: shared classification and emission plumbing. */
void lower_d_begin(lower_ctx *c, lower_demit *d, ek_buf *b);
void lower_d_end(lower_ctx *c, ek_buf *b);
lower_scalar lower_scalar_kind(const ek_type *t);
int lower_is_range(const ek_type *t);
void lower_d_printf(lower_demit *d, const char *fmt, ...);
const char *lower_d_temp(lower_demit *d);
void lower_d_concat_lit(lower_demit *d, const char *indent, const char *acc,
                        const char *lit);
void lower_d_concat_obj(lower_demit *d, const char *indent, const char *acc,
                        const char *part);
const char *lower_d_cstr(lower_ctx *c, const char *s);
int lower_shape_of(lower_ctx *c, const ek_node *at, const ek_type *t,
                   const char ***fnames, const ek_type ***ftypes, size_t *nf,
                   lower_gtype **g);
int lower_variants_of(lower_ctx *c, const ek_node *at, const ek_type *t,
                      lower_variant **out, size_t *nv);
const char *lower_std_ctor(lower_ctx *c, const ek_node *at,
                           const char *owner, const char *name);

/* typewalk.c: the applied type of a type-syntax node under a
   declaration's instantiation. */
const ek_type *lower_applied_type(lower_ctx *c, const ek_node *tnode,
                                  const ek_node *decl, const ek_type *owner);

/* derivedjson.c, derivedfrom.c: the JSON emitters and their shared
   spellings. OK_INFO is the typeinfo test of the std Result's Ok. */
#define LOWER_OK_INFO "&ekT_std_Result_Ok" 
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

/* derivedeq.c, derivedshow.c, derivedjson.c: the body emitters. */
void lower_dsvc_eq_emit(lower_ctx *c, const char *name, const ek_type *t);
void lower_dsvc_ord_emit(lower_ctx *c, const char *name, const ek_type *t);
void lower_dsvc_show_emit(lower_ctx *c, const char *name, const ek_type *t,
                          int inspect);
void lower_dsvc_json_emit(lower_ctx *c, const char *name, const ek_type *t,
                          int from);
void lower_dsvc_from_emit(lower_ctx *c, const char *name, const ek_type *t);

/* The form of a derived call: the receiver is the member link's node,
   the free call's first argument, or absent at a type head. */
typedef enum {
    CF_MEMBER, CF_FREE, CF_QUALIFIED
} lower_cmp_form;

/* derived.c: the service registry and the call-site routing. */
const char *lower_dsvc_request(lower_ctx *c, const ek_node *at,
                               lower_dsvc_kind kind, const ek_type *t);
lower_val lower_derived_call(lower_ctx *c, lower_scope *scope,
                             const ek_node *e, const ek_node *callee,
                             const ek_ck_row *frow, lower_cmp_form form);
lower_val lower_derived_with(lower_ctx *c, lower_scope *scope,
                             const ek_node *e, const ek_node *recv);
const char *lower_derived_cmp(lower_ctx *c, const ek_node *e, ek_binop op);
/* The service behind a comparison operator over the value layer's
   object slots (String, Bytes); NULL keeps the runtime entry. */

/* lower.c: shared helpers. */
void lower_emitf(lower_ctx *c, const char *fmt, ...);
void lower_pad(lower_ctx *c);
const char *lower_str(lower_ctx *c, const char *fmt, ...);
const char *lower_take(lower_ctx *c, ek_buf *b);
void *lower_grow(lower_ctx *c, void *data, size_t *cap, size_t need,
                 size_t width);
void lower_error(lower_ctx *c, const ek_node *at, const char *fmt, ...);
const char *surface_file(lower_ctx *c, const char *stem);
const ek_ck_row *lower_row(lower_ctx *c, const ek_node *n);
const ek_ref *lower_ref(lower_ctx *c, const ek_node *n);
void lower_enqueue(lower_ctx *c, const ek_symbol *sym);
void lower_clean_push(lower_ctx *c, const char *temp);
void lower_clean_pop(lower_ctx *c);
void lower_clean_release(lower_ctx *c, size_t base);
void lower_pending_add(lower_ctx *c, lower_pending *p);

/* tce.c: the self tail-call transform. */
int lower_has_self_tail(lower_ctx *c, const ek_symbol *self,
                        const ek_node *body);
void lower_emit_self_tail(lower_ctx *c, lower_scope *scope,
                          const ek_node *e, const ek_ref *ref);

/* name.c: the mangled function symbol, C spellings, string escapes. */
const char *lower_mangled(lower_ctx *c, const ek_symbol *sym);
const char *lower_temp(lower_ctx *c, lower_scope *scope);
const char *lower_c_name(lower_ctx *c, lower_scope *scope, const char *name);
void lower_string_literal(lower_ctx *c, ek_buf *b, const char *bytes,
                          size_t len);

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
lower_gtype *lower_anon_record(lower_ctx *c, const ek_type *rec);
lower_gtype *lower_anon_tuple(lower_ctx *c, size_t n);
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
void lower_emit_types(lower_ctx *c, ek_buf *unit);

/* composite.c: constructors and composite literals. */
lower_val lower_ctor_value(lower_ctx *c, const ek_node *e, const ek_ref *ref);
lower_val lower_ctor_apply(lower_ctx *c, lower_scope *scope,
                           const ek_node *e);
lower_val lower_composite(lower_ctx *c, lower_scope *scope,
                          const ek_node *e);

/* expr.c: expressions and construct naming for diagnostics. */
lower_val lower_expr(lower_ctx *c, lower_scope *scope, const ek_node *e);
lower_val lower_sub(lower_ctx *c, lower_scope *scope, const ek_node *e);
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

/* The out slot a status-returning runtime entry writes: a signed or
   unsigned word, a float, a code point, or a borrowed value. */
typedef enum {
    OUT_I64, OUT_U64, OUT_F64, OUT_U32, OUT_VAL
} out_kind;

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
                            const char *args, out_kind out);
lower_val lower_wrap_option_obj(lower_ctx *c, lower_scope *scope,
                                const ek_node *e, const char *sym,
                                const char *args);
lower_val lower_wrap_parse(lower_ctx *c, lower_scope *scope,
                           const ek_node *e, const char *sym,
                           const char *args, out_kind out, int radix);
lower_val lower_wrap_decode(lower_ctx *c, lower_scope *scope,
                            const ek_node *e, const char *sym,
                            const char *args);

/* capfam.c: the generated families the runtime builds over, registered
   as one set whenever the program touches the capability channel --
   the manifest supplies bundles and the advancing derivation builds
   one, and the archive members pull each other in, so one union covers
   every identity the runtime may name. The registrations stamp the
   identities without the construction helpers: the runtime builds the
   values itself. */
void lower_cap_families(lower_ctx *c, const ek_node *at);

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
void lower_function(lower_ctx *c, const ek_symbol *sym);
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

#endif
