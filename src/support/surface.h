#ifndef EK_SUPPORT_SURFACE_H
#define EK_SUPPORT_SURFACE_H

#include <stddef.h>

/* The compiler's shipped surface: one table holding every member the
   compiler supplies when no declaration owns the name. A row carries
   both halves -- the signature the checker types the member with, and
   the route the lowerer dispatches it over -- so a member joins the
   language in one place. The vocabulary type names (support/vocab.h)
   classify names; this table classifies members. */

/* World bit of a function type. result NULL means the arrow is
   elided and the result is `_`. */
typedef enum {
    EK_ARROW_PURE, EK_ARROW_WORLD, EK_ARROW_OPEN
} ek_arrow;

/* A row signature is a type tree: a fresh variable slot, a std
   nominal or one of an embedded module, a list, an Option or Result
   application, a nominal application, a function, or the unit type.
   Slots are per-consultation: every use of a row instantiates its own
   variables. */
typedef struct ek_sig ek_sig;

struct ek_sig {
    int kind;
    const char *std;       /* NS_STD, NS_EMB*: the type's name */
    const char *mod;       /* NS_EMB*: the embedded module's stem */
    int slot;              /* NS_VAR: the variable's slot */
    const ek_sig **params; /* NS_FN */
    size_t nparams;
    const ek_sig *result;
    ek_arrow arrow;        /* NS_FN: the arrow form */
    const ek_sig *arg;     /* NS_LIST, NS_OPTION, NS_RESULT, NS_APP*,
                              NS_EMB1, NS_DICT */
    const ek_sig *arg2;    /* NS_APP2, NS_DICT: the second argument */
};

enum { NS_VAR, NS_STD, NS_EMB, NS_EMB1, NS_FN, NS_UNIT, NS_LIST,
       NS_OPTION, NS_RESULT, NS_APP1, NS_APP2, NS_DICT };

#define SIG_STD(n)  &(ek_sig){.kind = NS_STD, .std = (n)}
/* A nominal of the system vocabulary module or of the capability
   module. */
#define SIG_SYS(n)  &(ek_sig){.kind = NS_EMB, .std = (n), .mod = "sys"}
#define SIG_CAP(n)  &(ek_sig){.kind = NS_EMB, .std = (n), .mod = "cap"}
#define SIG_VAR(i)  &(ek_sig){.kind = NS_VAR, .slot = (i)}
#define SIG_UNIT    &(ek_sig){.kind = NS_UNIT}
#define SIG_LIST(a) &(ek_sig){.kind = NS_LIST, .arg = (a)}
#define SIG_OPTION(a) &(ek_sig){.kind = NS_OPTION, .arg = (a)}
#define SIG_RESULT(a) &(ek_sig){.kind = NS_RESULT, .arg = (a)}

/* A std nominal applied to one type argument. */
#define SIG_APP1(n, a) &(ek_sig){.kind = NS_APP1, .std = (n), .arg = (a)}

/* A std nominal applied to two type arguments. */
#define SIG_APP2(n, a, b)                                             \
    &(ek_sig){.kind = NS_APP2, .std = (n), .arg = (a), .arg2 = (b)}

/* The structural dictionary [k]v. */
#define SIG_DICT(k, v)                                                \
    &(ek_sig){.kind = NS_DICT, .arg = (k), .arg2 = (v)}

/* An embedded module's nominal applied to one type argument. */
#define SIG_SYS1(n, a)                                                \
    &(ek_sig){.kind = NS_EMB1, .std = (n), .mod = "sys", .arg = (a)}
#define SIG_CAP1(n, a)                                                \
    &(ek_sig){.kind = NS_EMB1, .std = (n), .mod = "cap", .arg = (a)}

/* (params...) arrow result */
#define SIG_FN(ar, res, ...)                                          \
    &(ek_sig){.kind = NS_FN, .params = (const ek_sig *[]){__VA_ARGS__}, \
              .nparams = sizeof((const ek_sig *[]){__VA_ARGS__})       \
                          / sizeof(const ek_sig *),                    \
              .result = (res), .arrow = (ar)}

/* () arrow result */
#define SIG_FN0(ar, res)                                              \
    &(ek_sig){.kind = NS_FN, .params = NULL, .nparams = 0,            \
              .result = (res), .arrow = (ar)}

/* How the lowerer dispatches a row. */
typedef enum {
    EK_ROUTE_CALL,   /* a runtime call: sym over the argument letters */
    EK_ROUTE_CAP,    /* the capability channel's own wrapping */
    EK_ROUTE_UNWRAP, /* the panicking unwrap/expect pair */
    EK_ROUTE_CONST,  /* a constant of the namespace read as a value */
    EK_ROUTE_NONE    /* no route: the row types a name lower never
                        reaches through this table */
} ek_route;

/* The result wrapping of a runtime call. PLAIN is the table's
   default, so a plain row spells no wrapping. */
typedef enum {
    EK_WRAP_PLAIN,      /* the call value is the result */
    EK_WRAP_LEN,        /* a size_t count: the Int value of it */
    EK_WRAP_REF,        /* an owned object result: the reference value */
    EK_WRAP_KEEP,       /* the widening identity: the argument kept */
    EK_WRAP_BOOL,       /* a C truth word: the Bool value of it */
    EK_WRAP_SHOW,       /* a fresh display object over the value slots:
                           the reference value of it */
    EK_WRAP_OPTION,     /* status + out slot: Some payload or None */
    EK_WRAP_OPTION_OBJ, /* an owned object or NULL: Some payload or
                           None */
    EK_WRAP_PARSE,      /* parse status + out slot: Ok payload or Err
                           the ParseError of the kind */
    EK_WRAP_DECODE      /* ok flag + out object + detail: Ok payload or
                           Err (Invalid detail) */
} ek_wrap;

/* The slot a status contract writes its payload through. */
typedef enum { EK_OUT_I64, EK_OUT_U64, EK_OUT_F64, EK_OUT_U32,
               EK_OUT_VAL } ek_out;

/* The capability channel's result shapes. */
typedef enum {
    EK_CAP_PLAIN,
    EK_CAP_IO_REF,
    EK_CAP_IO_UNIT,
    EK_CAP_IO_OPT,
    EK_CAP_EXIT
} ek_cap_shape;

/* One member row. `name` NULL marks a namespace whose row set is
   complete: a name no station answered is absent, not bridged. A row
   reads its routing fields by the letter of `route` alone; CALL is
   the default route and PLAIN the default wrapping, so an ordinary
   runtime row spells only its symbol and its letters. */
typedef struct {
    const char *ns;
    const char *name;
    const ek_sig *sig;
    ek_route route;
    const char *sym;   /* EK_ROUTE_CALL, EK_ROUTE_CAP */
    const char *args;  /* one letter per runtime argument: v the value
                          slot, o the object, i the raw int64 word;
                          NULL is the single letter v */
    ek_wrap wrap;      /* EK_ROUTE_CALL */
    ek_out out;        /* EK_ROUTE_CALL */
    int radix;         /* EK_ROUTE_CALL: the radix parsers' base */
    ek_cap_shape shape; /* EK_ROUTE_CAP */
    int registers;     /* EK_ROUTE_CAP: the call constructs a
                          capability value whose bundle pull names
                          generated identities */
    const char *text;  /* EK_ROUTE_CONST: the C spelling of the value */
} ek_surface_row;

extern const ek_surface_row ek_surface_rows[];
extern const size_t ek_surface_rows_len;

/* The row of one namespace and member spelling, or NULL. */
const ek_surface_row *ek_surface_find(const char *ns, const char *member);

/* True when the namespace's row set is complete. */
int ek_surface_owned(const char *ns);

/* The binder count of a compiler-supplied std generic, zero for any
   other symbol. */
size_t ek_surface_generic_arity(const char *module, const char *name);

#endif
