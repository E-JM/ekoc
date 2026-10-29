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
} lower_gtype;

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

/* Lowering state. Function text accumulates in body; the entry point
   assembles the unit around it. work/seen drive reachability; file is
   the source file of the function being emitted; depth is the
   indentation level of the statement being emitted; in_tail marks a
   tail position of the function being emitted. */
typedef struct {
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
    size_t fn_count;
    lower_gtype *gtypes; /* the generated struct families, in first-use
                            order */
    size_t ngt, cgt;
    unsigned anon_rec; /* the anonymous record families so far */
    int uses_list; /* some body folds a list literal */
    int has_main;
    int oom;
} lower_ctx;

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
lower_val lower_string_expr(lower_ctx *c, const ek_node *e);

/* type.c: the generated struct layer. Registration happens while the
   bodies lower; the families themselves are emitted into the unit
   before the functions. */
lower_gtype *lower_decl_repr(lower_ctx *c, const ek_symbol *tsym);
lower_gtype *lower_anon_record(lower_ctx *c, const ek_type *rec);
lower_gtype *lower_anon_tuple(lower_ctx *c, size_t n);
lower_gtype *lower_ctor_family(lower_ctx *c, const ek_node *at,
                               const ek_ref *ref, const ek_symbol *tsym);
lower_gtype *lower_env_family(lower_ctx *c, const char **names, size_t n);
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
