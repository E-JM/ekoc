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
   spelling, and whether the binding is a borrowed parameter. */
typedef struct {
    const char *eko;
    const char *cname;
    int is_param;
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

/* Lowering state. Function text accumulates in body; the entry point
   assembles the unit around it. work/seen drive reachability; file is
   the source file of the function being emitted; depth is the
   indentation level of the statement being emitted. */
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
const char *lower_construct(const ek_node *n);

/* body.c: functions, statement members, and the if form. */
void lower_function(lower_ctx *c, const ek_symbol *sym);
void lower_if(lower_ctx *c, lower_scope *scope, const ek_node *e,
              lower_val *out);
const char *lower_as_temp(lower_ctx *c, lower_scope *scope, lower_val *v);
void lower_release_scope(lower_ctx *c, lower_scope *s);
lower_val lower_block_value(lower_ctx *c, lower_scope *scope,
                            const ek_node *body);
const lower_local *scope_find_local(lower_scope *scope, const char *name);

#endif
