#ifndef EK_RESOLVE_RESOLVE_H
#define EK_RESOLVE_RESOLVE_H

#include <stddef.h>

#include "parse/parse.h"
#include "support/arena.h"
#include "support/diag.h"

/*
 * AST -> symbols and bindings. ek_resolve is a pure entry point,
 * (input, arena, diag) -> output. It resolves one compilation root --
 * the module AST, its file, and the module path directories an import
 * searches -- and loads explicit imports recursively through
 * ek_lex/ek_layout/ek_parse; the dependency graph must be acyclic.
 * Errors go to diag, never abort.
 *
 * The output is dumb data: every resolved reference site carries the
 * symbol it denotes and the lookup order that produced it. refs are
 * sorted by node address, so check can binary search the table by AST
 * node. All storage lives in the arena and is valid until it is
 * destroyed.
 *
 * Receiver-form links whose reading needs the static type of the
 * receiver come back as EK_SYM_MEMBER: resolve verified every
 * name-level fact (the link is not a lexical binding, a module segment
 * names a real module, module-path members exist and are public);
 * check selects the field, namespace, or current-module reading from
 * the receiver's type.
 *
 * PLACEHOLDER STD SURFACE, standing in until the real interface is
 * derived from lib/std.eko. resolve answers the implicit `import std`
 * from a built-in surface holding the free names and type names it
 * opens. A name the placeholder does not know resolves to
 * EK_SYM_EXTERNAL with no diagnostic; this bridge must be replaced
 * when lib/std.eko lands. Explicit imports of user modules resolve
 * for real and are diagnosed.
 */

/* What a resolved name denotes; the trailing kinds carry data only
   check consumes. */
typedef enum {
    EK_SYM_PARAM,       /* lambda or `<-` parameter */
    EK_SYM_LOCAL,       /* `:=` or pattern binding inside a body */
    EK_SYM_FUNC,        /* top-level value or function binding */
    EK_SYM_TYPE,        /* `type` or `ctype` declaration */
    EK_SYM_CONSTRUCTOR, /* variant constructor; visibility follows the
                           type */
    EK_SYM_MODULE,      /* imported module, current module, or std */
    EK_SYM_NAMESPACE,   /* declared member of a type namespace */
    EK_SYM_TVAR,        /* type variable of an annotation */
    EK_SYM_TEST,        /* test binding; never enters the interface */
    EK_SYM_EXTERN,      /* extern declaration */
    EK_SYM_MEMBER,      /* receiver-form member; check selects the
                           reading */
    EK_SYM_EXTERNAL     /* unresolved external: the placeholder std
                           bridge described above */
} ek_sym_kind;

/* The lookup order that produced the symbol, one value per name layer
   or receiver form. */
typedef enum {
    EK_ST_HEAD_LOCAL,   /* expression head: parameter or local binding */
    EK_ST_HEAD_TOP,     /* expression head: file top-level binding (the
                           std free names share this layer) */
    EK_ST_HEAD_MODULE,  /* expression head: imported module */
    EK_ST_QUALIFIED,    /* Mod.f / Mod.Type / T.m at a head */
    EK_ST_TYPE,         /* type position: type variable or declared type
                           in unqualified scope */
    EK_ST_CONS,         /* constructor value or pattern position */
    EK_ST_FIELD,        /* bare `x.f`, a record field access */
    EK_ST_RECEIVER,     /* receiver form whose reading needs the
                           receiver's static type */
    EK_ST_MODULE_PATH,  /* `x.Mod.f(args)`: a module segment inside a
                           receiver chain */
    EK_ST_EXTERNAL      /* the placeholder std bridge */
} ek_station;

/* One symbol of a module surface. The interface data check needs
   travels here: the declaration node (binders, signatures, and generic
   bodies are reachable through it), the written annotation, visibility,
   the extern Pure status, and positions. */
typedef struct ek_symbol {
    const char *name;        /* declared spelling, `_` prefix included */
    ek_sym_kind kind;
    int public;              /* no leading `_` */
    const char *module;      /* declaring module stem */
    const char *owner;       /* namespace head type name, else NULL */
    const ek_node *decl;     /* declaration node; NULL for derived data */
    const ek_node *type;     /* written annotation, else NULL */
    const char *extern_symbol; /* extern C symbol, else NULL */
    int pure;                /* extern Pure status */
    int line;
    int col;
} ek_symbol;

/* The surface of one module in the closure. modules[0] of a result is
   the compilation root. ast is the module's own AST (NULL for the
   std placeholder), so check walks the root and reads the interface
   annotations of the modules it imports. */
typedef struct {
    const char *module; /* the stem, the module's name in code */
    const char *file;   /* path the module was loaded from */
    ek_symbol *syms;
    size_t count;
    const ek_node *ast;
} ek_surface;

/* One resolved reference site: an EK_N_NAME, EK_N_FIELD, EK_N_CALL, or
   EK_N_APPLY head, an EK_N_T_NAME in type position, or an EK_N_P_CONS
   in pattern position. symbol is NULL when kind is EK_SYM_MEMBER or
   EK_SYM_EXTERNAL, or when the reference is erroneous. */
typedef struct {
    const ek_node *node;
    ek_sym_kind kind;
    ek_station station;
    const ek_symbol *symbol;
} ek_ref;

typedef struct {
    ek_surface *modules; /* the closure; modules[0] is the root */
    size_t module_count;
    ek_ref *refs;        /* sorted by node address */
    size_t ref_count;
} ek_resolve_result;

/* Resolves module (an EK_N_MODULE of file) as a compilation root.
   path holds path_count module directories, searched in order.
   Diagnostics go to diag; a failed resolution still
   returns the partial tables. file and every path entry are borrowed. */
ek_resolve_result ek_resolve(const ek_node *module, const char *file,
                             const char *const *path, size_t path_count,
                             ek_arena *arena, ek_diag *diag);

#endif
