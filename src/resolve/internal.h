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

/* One local binding site, with the pattern that introduced it. The
   alternatives of one or-pattern share a group: they are one binding
   and a use of any alternative's symbol uses it. */
typedef struct {
    const ek_symbol *sym;
    const ek_node *group;
} r_local;

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
    size_t ref_begin, ref_end; /* this module's slice of the ref table */
    int state;
    int external; /* reserved std companion: a stem with no file */
} r_mod;

/* A free name an import opened into the file's top layer, with the
   import line that opened it so a declaration-point collision can name
   its position. at is NULL for the injected std import. */
typedef struct {
    const char *name;
    const ek_symbol *sym;
    const char *module;
    const ek_node *at;
} r_open;

/* The per-file name layers, in lookup order. Top-level bindings and
   the free names the imports opened share one layer; the parsed std
   module's surface supplies the type and constructor fallback below
   it, and the parsed capability module's surface the one below that. */
typedef struct {
    r_mod *mod;
    scope *locals; /* innermost lexical scope, NULL at top level */
    r_var *tops;   /* plain top-level bindings */
    size_t ntops, ctops;
    r_open *opens; /* free names opened by the imports */
    size_t nopens, copens;
    r_var *types;  /* identity layer: own + opened types */
    size_t ntypes, ctypes;
    r_var *ctors;  /* constructor multiset: own + opened */
    size_t nctors, cctors;
    r_var *mods;   /* module layer: own stem + imports + std */
    size_t nmods, cmods;
    const ek_surface *std; /* the implicit import, the bottom layer */
    const ek_surface *cap; /* the capability module, beneath std */
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
    ek_surface *std; /* the parsed std surface and its
                        placeholder rows */
    ek_surface *cap; /* the parsed cap surface and its
                        injected pair */
    r_local *locals; /* local binding sites, in declaration order */
    size_t nlocals, clocals;
    const ek_node *pat_group; /* the pattern whose binds are running */
    const ek_node *binding_node; /* the binding that owns the binds */
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
/* Adds the reference and attaches its constructor candidates. */
void r_add_ctor_ref(r_ctx *c, const ek_node *n, ek_sym_kind kind,
                    ek_station st, const ek_symbol *sym,
                    const ek_symbol *const *cands, size_t ncands);
void r_error(r_ctx *c, const char *file, int line, int col,
             const char *code, const char *fmt, ...);
void r_refs_sort(ek_ref *refs, size_t count);

/* warn.c: the unused-binding and unused-import warnings, and the
   deprecation warnings at use sites, over the finished closure. */
void r_warn_unused(r_ctx *c);
void r_warn_deprecated(r_ctx *c);

/* std.c: the parsed std module, its placeholder rows, and the implicit
   import. r_std_boot parses the embedded source into the closure and
   extends its surface; r_std_open binds the injected free names. */
void r_std_boot(r_ctx *c);
int r_std_opened_name(const char *name);
void r_std_open(r_ctx *c, r_file *f);
const ek_symbol *r_std_find(const ek_surface *s, ek_sym_kind kind,
                            const char *name);
int r_is_placeholder(const ek_symbol *sym);

/* cap.c: the capability module, parsed from the embedded cap/cap.eko
   source. r_cap_boot parses the embedded source into the closure and
   appends the injected pair; r_cap_open binds the per-file fallback
   beneath std. */
void r_cap_boot(r_ctx *c);
void r_cap_open(r_ctx *c, r_file *f);

/* opens.c: the per-file opened free-name table. One import's list is
   validated against the target surface and each eligible name enters
   the file's top layer; duplicates are declaration-point conflicts. */
void r_open_name(r_ctx *c, r_file *f, const char *name,
                 const ek_symbol *sym, const char *module,
                 const ek_node *at);
void r_opened_import(r_ctx *c, r_file *f, const r_mod *target,
                     const ek_node *imp);
void r_opened_vs_tops(r_ctx *c, r_file *f);

/* surface.c: the top-level surface of one module. */
void r_surface_build(r_ctx *c, r_mod *m);

/* module.c: file loading and the import closure. r_parse_source is the
   normal lex/layout/parse chain over one source buffer; the std module
   takes it with its embedded bytes. */
char *r_path_stem(r_ctx *c, const char *file);
r_mod *r_module_find(r_ctx *c, const char *stem);
r_mod *r_module_add(r_ctx *c, const char *stem, const char *file,
                    const ek_node *ast);
r_mod *r_module_companion(r_ctx *c, const char *stem);
ek_node *r_parse_source(r_ctx *c, const char *src, size_t len,
                        const char *path);
void r_closure_walk(r_ctx *c, r_mod *m);
void r_closure(r_ctx *c, r_mod *root);

/* type.c: type positions. */
void r_type(r_ctx *c, r_file *f, const ek_node *t);

/* body.c: blocks, bindings, patterns, case, lambda. */
void r_block(r_ctx *c, r_file *f, const ek_node *block, scope *parent);
void r_body(r_ctx *c, r_file *f, const ek_node *body, scope *parent);
void r_lambda(r_ctx *c, r_file *f, const ek_node *lam);
void r_case(r_ctx *c, r_file *f, const ek_node *cs);
void r_pattern_binds(r_ctx *c, r_file *f, const ek_node *pat);
void r_pattern_dup(r_ctx *c, r_file *f, const ek_node *pat);
void r_pattern_refs(r_ctx *c, r_file *f, const ek_node *pat);
void r_tops(r_ctx *c, r_file *f, const ek_node *mod);

/* expr.c: expressions, chains, calls. */
void r_expr(r_ctx *c, r_file *f, const ek_node *e);

/* The layer lookups, shared with chain.c and type.c. A free name
   answers from the file's own bindings or the opened table; the std
   surface still supplies the opened types and constructors.
   r_lookup_type counts visible same-named entries: ambiguity is an
   error at the use site. r_lookup_ctor returns every visible
   same-named constructor, user candidates first and the std
   placeholder last, in a fresh arena array. */
const ek_symbol *r_lookup_local(const r_file *f, const char *name);
const ek_symbol *r_lookup_top(const r_file *f, const char *name);
const ek_symbol *r_lookup_mod(const r_file *f, const char *name);
size_t r_lookup_type(const r_file *f, const char *name,
                     const ek_symbol **first);
size_t r_lookup_ctor(r_ctx *c, const r_file *f, const char *name,
                     const ek_symbol *const **cands);

/* What one name or constructor head denotes. */
typedef struct {
    const ek_symbol *sym; /* NULL on error or unknown external */
    ek_sym_kind kind;
    ek_station st;
} r_head;

/* Expression head: parameter, local, top-level, module; a Pascal name
   is a constructor. chained admits the module reading for a qualified
   head. */
void r_name_head(r_ctx *c, r_file *f, const ek_node *n, int chained,
                 r_head *out);

/* Constructor value reading of an all-Pascal APPLY head. */
void r_cons_head(r_ctx *c, r_file *f, const ek_node *n, r_head *out);

/* chain.c: dotted references and receiver chains. node is the top
   EK_N_FIELD or EK_N_CALL; out carries the symbol the whole chain
   denotes. */
void r_chain(r_ctx *c, r_file *f, const ek_node *node, int is_call,
             r_head *out);
const ek_symbol *r_module_member(const r_mod *m, const char *name);
const ek_symbol *r_module_ctor(const r_mod *m, const char *name);

#endif
