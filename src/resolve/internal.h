#ifndef EK_RESOLVE_INTERNAL_H
#define EK_RESOLVE_INTERNAL_H

#include <stddef.h>

#include "resolve/resolve.h"

/* Resolver state shared by the files of this directory. One ek_resolve
   call resolves one closure; every array grows in the arena. */

typedef struct {
    const char *name;
    const ek_symbol *sym;
} r_var;

typedef struct scope scope;
struct scope {
    scope *parent;
    r_var *vars;
    size_t count, cap;
};

/* One module of the closure. state: 0 new, 1 loading, 2 done. */
typedef struct {
    const char *stem;
    const char *file;
    const ek_node *ast; /* EK_N_MODULE */
    ek_surface *surf;
    int state;
    int external; /* placeholder std module: no file, no surface */
} r_mod;

/* The per-file name layers of Clause 7.4.2, in lookup order. */
typedef struct {
    r_mod *mod;
    scope *locals; /* innermost lexical scope, NULL at top level */
    r_var *tops;   /* plain top-level bindings + std free names */
    size_t ntops, ctops;
    r_var *types;  /* identity layer: own + opened + std types */
    size_t ntypes, ctypes;
    r_var *ctors;  /* constructor multiset: own + opened + std */
    size_t nctors, cctors;
    r_var *mods;   /* module layer: own stem + imports + std */
    size_t nmods, cmods;
} r_file;

typedef struct {
    ek_arena *arena;
    ek_diag *diag;
    const char *const *path;
    size_t path_count;
    r_mod *mods; /* the closure, root first */
    size_t nmods, cmods;
    ek_ref *refs;
    size_t nrefs, crefs;
    ek_surface *std; /* the placeholder surface of Clause 7.2.5 */
    int oom;
} r_ctx;

/* scope.c: arena growth, scopes, symbols, refs, case classes. */
void *r_grow(r_ctx *c, void *old, size_t n, size_t *cap, size_t elem);
int r_is_pascal(const char *s);
int r_is_snake(const char *s);
ek_symbol *r_sym(r_ctx *c, const char *name, ek_sym_kind kind,
                 const char *module, int line, int col);
scope *r_scope_new(r_ctx *c, scope *parent);
void r_scope_add(r_ctx *c, scope *s, const char *name, const ek_symbol *sym);
const ek_symbol *r_scope_get(const scope *s, const char *name);
void r_add_ref(r_ctx *c, const ek_node *n, ek_sym_kind kind,
               ek_station st, const ek_symbol *sym);
void r_error(r_ctx *c, const char *file, int line, int col,
             const char *code, const char *fmt, ...);
void r_refs_sort(ek_ref *refs, size_t count);

/* std.c: the placeholder surface of Clause 7.2.5 (temporary bridge). */
void r_std_init(r_ctx *c);
int r_std_free_name(const char *name);
void r_std_open(r_ctx *c, r_file *f);
const ek_symbol *r_std_type(r_ctx *c, const char *name);
const ek_symbol *r_std_ctor(r_ctx *c, const char *name);

/* surface.c: the top-level surface of one module (Clause 7.3). */
void r_surface_build(r_ctx *c, r_mod *m);

/* module.c: file loading and the import closure (Clause 7.2). */
char *r_path_stem(r_ctx *c, const char *file);
r_mod *r_module_find(r_ctx *c, const char *stem);
void r_closure_walk(r_ctx *c, r_mod *m);
void r_closure(r_ctx *c, r_mod *root);

/* type.c: type position (Clauses 7.4.1, 7.4.2, 7.6.1). */
void r_type(r_ctx *c, r_file *f, const ek_node *t);

/* body.c: blocks, bindings, patterns, case, lambda (Clauses 7.4.2,
   7.7.1). */
void r_block(r_ctx *c, r_file *f, const ek_node *block, scope *parent);
void r_body(r_ctx *c, r_file *f, const ek_node *body, scope *parent);
void r_lambda(r_ctx *c, r_file *f, const ek_node *lam);
void r_case(r_ctx *c, r_file *f, const ek_node *cs);
void r_pattern_binds(r_ctx *c, r_file *f, const ek_node *pat);
void r_pattern_dup(r_ctx *c, r_file *f, const ek_node *pat);
void r_pattern_refs(r_ctx *c, r_file *f, const ek_node *pat);
void r_tops(r_ctx *c, r_file *f, const ek_node *mod);

/* expr.c: expressions, chains, calls (Clauses 7.4.3, 7.4.4). */
void r_expr(r_ctx *c, r_file *f, const ek_node *e);

/* The layer lookups of Clause 7.4.2, shared with chain.c and type.c.
   r_lookup_type and r_lookup_ctor count visible same-named entries:
   ambiguity is an error at the use site (Clauses 7.4.5, 7.4.6). */
const ek_symbol *r_lookup_local(const r_file *f, const char *name);
const ek_symbol *r_lookup_top(const r_file *f, const char *name);
const ek_symbol *r_lookup_mod(const r_file *f, const char *name);
size_t r_lookup_type(const r_file *f, const char *name,
                     const ek_symbol **first);
size_t r_lookup_ctor(const r_file *f, const char *name,
                     const ek_symbol **first);

/* What one name or constructor head denotes. */
typedef struct {
    const ek_symbol *sym; /* NULL on error or unknown external */
    ek_sym_kind kind;
    ek_station st;
} r_head;

/* Expression head: parameter, local, top-level, module (7.4.2); a
   Pascal name is a constructor (7.4.1). chained admits the module
   reading for a qualified head. */
void r_name_head(r_ctx *c, r_file *f, const ek_node *n, int chained,
                 r_head *out);

/* Constructor value reading of an all-Pascal APPLY head (7.4.1). */
void r_cons_head(r_ctx *c, r_file *f, const ek_node *n, r_head *out);

/* chain.c: dotted references and receiver chains (Clauses 7.4.3,
   7.4.4). node is the top EK_N_FIELD or EK_N_CALL; out carries the
   symbol the whole chain denotes. */
void r_chain(r_ctx *c, r_file *f, const ek_node *node, int is_call,
             r_head *out);
const ek_symbol *r_module_member(const r_mod *m, const char *name);
const ek_symbol *r_module_ctor(const r_mod *m, const char *name);

#endif
