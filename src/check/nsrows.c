#include "check/internal.h"

/* The compiler-provided member rows of the std namespaces: the table
   the consult machinery of namespace.c instantiates. A declared
   namespace member wins over a row, and the derived services win over
   both, so a row is the default of a name no declaration owns. A
   namespace the table owns is complete: a name no station answers is
   absent, not bridged. */

#define SIG_STD(n)  &(ns_sig){.kind = NS_STD, .std = (n)}
/* A nominal of the system vocabulary module or of the capability
   module. */
#define SIG_SYS(n)  &(ns_sig){.kind = NS_EMB, .std = (n), .mod = "sys"}
#define SIG_CAP(n)  &(ns_sig){.kind = NS_EMB, .std = (n), .mod = "cap"}
#define SIG_VAR(i)  &(ns_sig){.kind = NS_VAR, .slot = (i)}
#define SIG_UNIT    &(ns_sig){.kind = NS_UNIT}
#define SIG_LIST(a) &(ns_sig){.kind = NS_LIST, .arg = (a)}
#define SIG_OPTION(a) &(ns_sig){.kind = NS_OPTION, .arg = (a)}
#define SIG_RESULT(a) &(ns_sig){.kind = NS_RESULT, .arg = (a)}

/* A std nominal applied to one type argument. */
#define SIG_APP1(n, a) &(ns_sig){.kind = NS_APP1, .std = (n), .arg = (a)}

/* A std nominal applied to two type arguments. */
#define SIG_APP2(n, a, b)                                             \
    &(ns_sig){.kind = NS_APP2, .std = (n), .arg = (a), .arg2 = (b)}

/* The structural dictionary [k]v. */
#define SIG_DICT(k, v)                                                \
    &(ns_sig){.kind = NS_DICT, .arg = (k), .arg2 = (v)}

/* An embedded module's nominal applied to one type argument: `Task b`
   of the concurrency surface, the generic file bundle. */
#define SIG_SYS1(n, a)                                                \
    &(ns_sig){.kind = NS_EMB1, .std = (n), .mod = "sys", .arg = (a)}
#define SIG_CAP1(n, a)                                                \
    &(ns_sig){.kind = NS_EMB1, .std = (n), .mod = "cap", .arg = (a)}

/* (params...) arrow result */
#define SIG_FN(ar, res, ...)                                          \
    &(ns_sig){.kind = NS_FN, .params = (const ns_sig *[]){__VA_ARGS__}, \
              .nparams = sizeof((const ns_sig *[]){__VA_ARGS__})       \
                          / sizeof(const ns_sig *),                    \
              .result = (res), .arrow = (ar)}

/* () arrow result */
#define SIG_FN0(ar, res)                                              \
    &(ns_sig){.kind = NS_FN, .params = NULL, .nparams = 0,            \
              .result = (res), .arrow = (ar)}

/* The members the compiler supplies when no declaration owns the
   name. This table is the intrinsic boundary of the shipped surface:
   the scalar arithmetic, conversion, formatting, and parsing rows; the
   String scalar access and the Bytes operations; the Dict, Set, Range,
   and Path operations; the time parsers; Json.parse; List.append; the
   panic-coded unwrap/expect pair; and the compiler-provided capability
   rows. Every other inventory member is std, cap, or companion
   source. */
const ns_row ck_ns_rows[] = {
    /* Bool.not is std source; the logical operators are sugar. */

    {"Float", "add", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                            SIG_STD("Float"), SIG_STD("Float"))},
    {"Float", "div", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                            SIG_STD("Float"), SIG_STD("Float"))},
    {"Float", "mul", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                            SIG_STD("Float"), SIG_STD("Float"))},
    {"Float", "neg", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                            SIG_STD("Float"))},
    {"Float", "rem", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                            SIG_STD("Float"), SIG_STD("Float"))},
    {"Float", "sub", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                            SIG_STD("Float"), SIG_STD("Float"))},

    /* Conversions across domains, the parse family, and formatting. */
    {"Float", "to_int", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("Int")),
                              SIG_STD("Float"))},
    {"Float", "to_uint", SIG_FN(EK_ARROW_PURE,
                               SIG_OPTION(SIG_STD("UInt")),
                               SIG_STD("Float"))},
    {"Float", "parse", SIG_FN(EK_ARROW_PURE, SIG_RESULT(SIG_STD("Float")),
                             SIG_STD("String"))},
    {"Float", "to_fixed", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                SIG_STD("Float"), SIG_STD("UInt"))},
    {"Float", "to_exp", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                              SIG_STD("Float"), SIG_STD("UInt"))},

    /* The floating-point mathematics surface. */
    {"Float", "abs", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                           SIG_STD("Float"))},
    {"Float", "floor", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                             SIG_STD("Float"))},
    {"Float", "ceil", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                            SIG_STD("Float"))},
    {"Float", "trunc", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                             SIG_STD("Float"))},
    {"Float", "round", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                             SIG_STD("Float"))},
    {"Float", "pow", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                           SIG_STD("Float"), SIG_STD("Float"))},
    {"Float", "sqrt", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                            SIG_STD("Float"))},
    {"Float", "exp", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                           SIG_STD("Float"))},
    {"Float", "log", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                           SIG_STD("Float"))},
    {"Float", "log2", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                            SIG_STD("Float"))},
    {"Float", "log10", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                             SIG_STD("Float"))},
    {"Float", "sin", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                           SIG_STD("Float"))},
    {"Float", "cos", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                           SIG_STD("Float"))},
    {"Float", "tan", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                           SIG_STD("Float"))},
    {"Float", "asin", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                            SIG_STD("Float"))},
    {"Float", "acos", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                            SIG_STD("Float"))},
    {"Float", "atan", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                            SIG_STD("Float"))},
    {"Float", "atan2", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                             SIG_STD("Float"), SIG_STD("Float"))},
    {"Float", "is_nan", SIG_FN(EK_ARROW_PURE, SIG_STD("Bool"),
                              SIG_STD("Float"))},
    {"Float", "is_inf", SIG_FN(EK_ARROW_PURE, SIG_STD("Bool"),
                              SIG_STD("Float"))},
    {"Float", "is_finite", SIG_FN(EK_ARROW_PURE, SIG_STD("Bool"),
                                 SIG_STD("Float"))},

    /* The machine-word sizes: narrowing is checked and the float
       widths are total. */
    {"Float", "to_i8", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("I8")),
                             SIG_STD("Float"))},
    {"Float", "to_i16", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("I16")),
                              SIG_STD("Float"))},
    {"Float", "to_i32", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("I32")),
                              SIG_STD("Float"))},
    {"Float", "to_i64", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("I64")),
                              SIG_STD("Float"))},
    {"Float", "to_u8", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("U8")),
                             SIG_STD("Float"))},
    {"Float", "to_u16", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("U16")),
                              SIG_STD("Float"))},
    {"Float", "to_u32", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("U32")),
                              SIG_STD("Float"))},
    {"Float", "to_u64", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("U64")),
                              SIG_STD("Float"))},
    {"Float", "to_f32", SIG_FN(EK_ARROW_PURE, SIG_STD("F32"),
                              SIG_STD("Float"))},
    {"Float", "to_f64", SIG_FN(EK_ARROW_PURE, SIG_STD("F64"),
                              SIG_STD("Float"))},
    {"Float", "from_f32", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                                SIG_STD("F32"))},
    {"Float", "from_f64", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                                SIG_STD("F64"))},
    {"Int", "abs", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                          SIG_STD("Int"))},
    {"Int", "add", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                          SIG_STD("Int"), SIG_STD("Int"))},
    {"Int", "div", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                          SIG_STD("Int"), SIG_STD("Int"))},
    {"Int", "max", SIG_STD("Int")},
    {"Int", "min", SIG_STD("Int")},
    {"Int", "mod", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                          SIG_STD("Int"), SIG_STD("Int"))},
    {"Int", "mul", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                          SIG_STD("Int"), SIG_STD("Int"))},
    {"Int", "neg", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                          SIG_STD("Int"))},
    {"Int", "pow", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                          SIG_STD("Int"), SIG_STD("UInt"))},
    {"Int", "quot", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                           SIG_STD("Int"), SIG_STD("Int"))},
    {"Int", "rem", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                          SIG_STD("Int"), SIG_STD("Int"))},
    {"Int", "sub", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                          SIG_STD("Int"), SIG_STD("Int"))},

    /* Conversions across domains, the parse family, and formatting. */
    {"Int", "to_uint", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("UInt")),
                             SIG_STD("Int"))},
    {"Int", "to_float", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                              SIG_STD("Int"))},
    {"Int", "parse", SIG_FN(EK_ARROW_PURE, SIG_RESULT(SIG_STD("Int")),
                           SIG_STD("String"))},
    {"Int", "to_hex", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                            SIG_STD("Int"))},
    {"Int", "to_oct", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                            SIG_STD("Int"))},
    {"Int", "to_bin", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                            SIG_STD("Int"))},
    {"Int", "parse_hex", SIG_FN(EK_ARROW_PURE, SIG_RESULT(SIG_STD("Int")),
                               SIG_STD("String"))},
    {"Int", "parse_oct", SIG_FN(EK_ARROW_PURE, SIG_RESULT(SIG_STD("Int")),
                               SIG_STD("String"))},
    {"Int", "parse_bin", SIG_FN(EK_ARROW_PURE, SIG_RESULT(SIG_STD("Int")),
                               SIG_STD("String"))},

    /* Wrapping arithmetic on the machine ring. */
    {"Int", "wrapping_add", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                                  SIG_STD("Int"), SIG_STD("Int"))},
    {"Int", "wrapping_sub", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                                  SIG_STD("Int"), SIG_STD("Int"))},
    {"Int", "wrapping_mul", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                                  SIG_STD("Int"), SIG_STD("Int"))},
    {"Int", "wrapping_neg", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                                  SIG_STD("Int"))},

    /* The machine-word sizes: the to_ family checks the range, and
       from_ covers only the machine domains the word includes. */
    {"Int", "to_i8", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("I8")),
                            SIG_STD("Int"))},
    {"Int", "to_i16", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("I16")),
                             SIG_STD("Int"))},
    {"Int", "to_i32", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("I32")),
                             SIG_STD("Int"))},
    {"Int", "to_i64", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("I64")),
                             SIG_STD("Int"))},
    {"Int", "to_u8", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("U8")),
                            SIG_STD("Int"))},
    {"Int", "to_u16", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("U16")),
                             SIG_STD("Int"))},
    {"Int", "to_u32", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("U32")),
                             SIG_STD("Int"))},
    {"Int", "to_u64", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("U64")),
                             SIG_STD("Int"))},
    {"Int", "to_f32", SIG_FN(EK_ARROW_PURE, SIG_STD("F32"),
                             SIG_STD("Int"))},
    {"Int", "to_f64", SIG_FN(EK_ARROW_PURE, SIG_STD("F64"),
                             SIG_STD("Int"))},
    {"Int", "from_i8", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                              SIG_STD("I8"))},
    {"Int", "from_i16", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                               SIG_STD("I16"))},
    {"Int", "from_i32", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                               SIG_STD("I32"))},
    {"Int", "from_i64", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                               SIG_STD("I64"))},
    {"Int", "from_u8", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                              SIG_STD("U8"))},
    {"Int", "from_u16", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                               SIG_STD("U16"))},
    {"Int", "from_u32", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                               SIG_STD("U32"))},

    /* The List namespace: only append is compiler-supplied, because a
       list has no source-level cons form; the std source declares the
       rest against the inductive patterns. */
    {"List", "append", SIG_FN(EK_ARROW_PURE, SIG_LIST(SIG_VAR(0)),
                              SIG_LIST(SIG_VAR(0)), SIG_LIST(SIG_VAR(0)))},

    /* The Dict namespace over the structural dictionary: the key and
       value variables stay paired across every member, and the
       callbacks take both. */
    {"Dict", "length", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                              SIG_DICT(SIG_VAR(0), SIG_VAR(1)))},
    {"Dict", "get", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_VAR(1)),
                           SIG_DICT(SIG_VAR(0), SIG_VAR(1)), SIG_VAR(0))},
    {"Dict", "insert", SIG_FN(EK_ARROW_PURE,
                              SIG_DICT(SIG_VAR(0), SIG_VAR(1)),
                              SIG_DICT(SIG_VAR(0), SIG_VAR(1)), SIG_VAR(0),
                              SIG_VAR(1))},
    {"Dict", "remove", SIG_FN(EK_ARROW_PURE,
                              SIG_DICT(SIG_VAR(0), SIG_VAR(1)),
                              SIG_DICT(SIG_VAR(0), SIG_VAR(1)), SIG_VAR(0))},
    {"Dict", "merge", SIG_FN(EK_ARROW_PURE,
                             SIG_DICT(SIG_VAR(0), SIG_VAR(1)),
                             SIG_DICT(SIG_VAR(0), SIG_VAR(1)),
                             SIG_DICT(SIG_VAR(0), SIG_VAR(1)))},
    {"Dict", "map", SIG_FN(EK_ARROW_OPEN,
                            SIG_DICT(SIG_VAR(0), SIG_VAR(2)),
                            SIG_DICT(SIG_VAR(0), SIG_VAR(1)),
                            SIG_FN(EK_ARROW_OPEN, SIG_VAR(2), SIG_VAR(0),
                                   SIG_VAR(1)))},
    {"Dict", "filter", SIG_FN(EK_ARROW_OPEN,
                               SIG_DICT(SIG_VAR(0), SIG_VAR(1)),
                               SIG_DICT(SIG_VAR(0), SIG_VAR(1)),
                               SIG_FN(EK_ARROW_OPEN, SIG_STD("Bool"),
                                      SIG_VAR(0), SIG_VAR(1)))},
    {"Dict", "fold", SIG_FN(EK_ARROW_OPEN, SIG_VAR(2),
                             SIG_DICT(SIG_VAR(0), SIG_VAR(1)), SIG_VAR(2),
                             SIG_FN(EK_ARROW_OPEN, SIG_VAR(2), SIG_VAR(2),
                                    SIG_VAR(0), SIG_VAR(1)))},
    {"Dict", "each", SIG_FN(EK_ARROW_OPEN, SIG_UNIT,
                             SIG_DICT(SIG_VAR(0), SIG_VAR(1)),
                             SIG_FN(EK_ARROW_OPEN, SIG_UNIT, SIG_VAR(0),
                                    SIG_VAR(1)))},

    /* The Set namespace: the element variable of the receiver ties
       the argument and the result. */
    {"Set", "of", SIG_FN(EK_ARROW_PURE, SIG_APP1("Set", SIG_VAR(0)),
                          SIG_LIST(SIG_VAR(0)))},
    {"Set", "to_list", SIG_FN(EK_ARROW_PURE, SIG_LIST(SIG_VAR(0)),
                               SIG_APP1("Set", SIG_VAR(0)))},
    {"Set", "contains", SIG_FN(EK_ARROW_PURE, SIG_STD("Bool"),
                               SIG_APP1("Set", SIG_VAR(0)), SIG_VAR(0))},
    {"Set", "insert", SIG_FN(EK_ARROW_PURE,
                              SIG_APP1("Set", SIG_VAR(0)),
                              SIG_APP1("Set", SIG_VAR(0)), SIG_VAR(0))},
    {"Set", "remove", SIG_FN(EK_ARROW_PURE,
                              SIG_APP1("Set", SIG_VAR(0)),
                              SIG_APP1("Set", SIG_VAR(0)), SIG_VAR(0))},
    {"Set", "length", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                              SIG_APP1("Set", SIG_VAR(0)))},
    {"Set", "union", SIG_FN(EK_ARROW_PURE, SIG_APP1("Set", SIG_VAR(0)),
                             SIG_APP1("Set", SIG_VAR(0)),
                             SIG_APP1("Set", SIG_VAR(0)))},
    {"Set", "intersect", SIG_FN(EK_ARROW_PURE,
                                 SIG_APP1("Set", SIG_VAR(0)),
                                 SIG_APP1("Set", SIG_VAR(0)),
                                 SIG_APP1("Set", SIG_VAR(0)))},
    {"Set", "diff", SIG_FN(EK_ARROW_PURE, SIG_APP1("Set", SIG_VAR(0)),
                            SIG_APP1("Set", SIG_VAR(0)),
                            SIG_APP1("Set", SIG_VAR(0)))},

    /* The String namespace: the scalar sequence primitives. The
       composition (split, trim, replace, slice, padding) is std
       source over these. */
    {"String", "length", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                                SIG_STD("String"))},
    {"String", "chars", SIG_FN(EK_ARROW_PURE, SIG_LIST(SIG_STD("Char")),
                               SIG_STD("String"))},
    {"String", "from_char", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                   SIG_STD("Char"))},
    {"String", "from_chars", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                    SIG_LIST(SIG_STD("Char")))},
    {"String", "append", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                SIG_STD("String"), SIG_STD("String"))},
    {"String", "to_bytes", SIG_FN(EK_ARROW_PURE, SIG_STD("Bytes"),
                                  SIG_STD("String"))},

    /* The Char namespace: the code-point bridge in both directions,
       the ASCII classifiers, the space property, and the XID pair. */
    {"Char", "to_code", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                               SIG_STD("Char"))},
    {"Char", "from_code", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("Char")),
                                 SIG_STD("Int"))},
    {"Char", "is_alpha", SIG_FN(EK_ARROW_PURE, SIG_STD("Bool"),
                                SIG_STD("Char"))},
    {"Char", "is_digit", SIG_FN(EK_ARROW_PURE, SIG_STD("Bool"),
                                SIG_STD("Char"))},
    {"Char", "is_alnum", SIG_FN(EK_ARROW_PURE, SIG_STD("Bool"),
                                SIG_STD("Char"))},
    {"Char", "is_space", SIG_FN(EK_ARROW_PURE, SIG_STD("Bool"),
                                SIG_STD("Char"))},
    {"Char", "is_upper", SIG_FN(EK_ARROW_PURE, SIG_STD("Bool"),
                                SIG_STD("Char"))},
    {"Char", "is_lower", SIG_FN(EK_ARROW_PURE, SIG_STD("Bool"),
                                SIG_STD("Char"))},
    {"Char", "is_xid_start", SIG_FN(EK_ARROW_PURE, SIG_STD("Bool"),
                                    SIG_STD("Char"))},
    {"Char", "is_xid_continue", SIG_FN(EK_ARROW_PURE, SIG_STD("Bool"),
                                       SIG_STD("Char"))},

    /* The Bytes namespace: the byte sequence, its integer and word
       bridges, and the hexadecimal and text decoders. */
    {"Bytes", "length", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                               SIG_STD("Bytes"))},
    {"Bytes", "get", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("Int")),
                            SIG_STD("Bytes"), SIG_STD("Int"))},
    {"Bytes", "append", SIG_FN(EK_ARROW_PURE, SIG_STD("Bytes"),
                               SIG_STD("Bytes"), SIG_STD("Bytes"))},
    {"Bytes", "to_ints", SIG_FN(EK_ARROW_PURE, SIG_LIST(SIG_STD("Int")),
                                SIG_STD("Bytes"))},
    {"Bytes", "from_ints", SIG_FN(EK_ARROW_PURE,
                                  SIG_OPTION(SIG_STD("Bytes")),
                                  SIG_LIST(SIG_STD("Int")))},
    {"Bytes", "to_u8s", SIG_FN(EK_ARROW_PURE, SIG_LIST(SIG_STD("U8")),
                               SIG_STD("Bytes"))},
    {"Bytes", "from_u8s", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("Bytes")),
                                 SIG_LIST(SIG_STD("U8")))},
    {"Bytes", "to_hex", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                               SIG_STD("Bytes"))},
    {"Bytes", "from_hex", SIG_FN(EK_ARROW_PURE, SIG_RESULT(SIG_STD("Bytes")),
                                 SIG_STD("String"))},
    {"Bytes", "to_string", SIG_FN(EK_ARROW_PURE,
                                  SIG_RESULT(SIG_STD("String")),
                                  SIG_STD("Bytes"))},

    /* The Option namespace: the combinators are std source; the
       panicking observations carry the fixed P-codes and stay here. */
    {"Option", "unwrap", SIG_FN(EK_ARROW_PURE, SIG_VAR(0),
                                SIG_OPTION(SIG_VAR(0)))},
    {"Option", "expect", SIG_FN(EK_ARROW_PURE, SIG_VAR(0),
                                SIG_OPTION(SIG_VAR(0)), SIG_STD("String"))},

    /* The Result namespace: the combinators are std source; the
       panicking observations carry the fixed P-codes and stay here. */
    {"Result", "unwrap", SIG_FN(EK_ARROW_PURE, SIG_VAR(0),
                                SIG_APP2("Result", SIG_VAR(0),
                                         SIG_VAR(1)))},
    {"Result", "expect", SIG_FN(EK_ARROW_PURE, SIG_VAR(0),
                                SIG_APP2("Result", SIG_VAR(0),
                                         SIG_VAR(1)), SIG_STD("String"))},

    /* The Range namespace over the range ADT: the endpoint variable
       ties the receiver to the element of the produced list. */
    {"Range", "to_list", SIG_FN(EK_ARROW_PURE, SIG_LIST(SIG_VAR(0)),
                                SIG_APP1("Range", SIG_VAR(0)))},
    {"Range", "length", SIG_FN(EK_ARROW_PURE, SIG_STD("Int"),
                               SIG_APP1("Range", SIG_VAR(0)))},
    {"Range", "each", SIG_FN(EK_ARROW_OPEN, SIG_UNIT,
                             SIG_APP1("Range", SIG_VAR(0)),
                             SIG_FN(EK_ARROW_OPEN, SIG_UNIT,
                                    SIG_VAR(0)))},
    {"Range", "each_with_index", SIG_FN(EK_ARROW_OPEN, SIG_UNIT,
                                        SIG_APP1("Range", SIG_VAR(0)),
                                        SIG_FN(EK_ARROW_OPEN, SIG_UNIT,
                                               SIG_VAR(0), SIG_STD("Int")))},
    {"Range", "map", SIG_FN(EK_ARROW_OPEN, SIG_LIST(SIG_VAR(1)),
                            SIG_APP1("Range", SIG_VAR(0)),
                            SIG_FN(EK_ARROW_OPEN, SIG_VAR(1),
                                   SIG_VAR(0)))},
    {"Range", "fold", SIG_FN(EK_ARROW_OPEN, SIG_VAR(1),
                             SIG_APP1("Range", SIG_VAR(0)), SIG_VAR(1),
                             SIG_FN(EK_ARROW_OPEN, SIG_VAR(1), SIG_VAR(1),
                                    SIG_VAR(0)))},

    /* The wall-clock namespace: the printed form's parser alone; the
       arithmetic and the payload are companion source (std/time). */
    {"Time", "parse", SIG_FN(EK_ARROW_PURE, SIG_RESULT(SIG_STD("Time")),
                              SIG_STD("String"))},

    /* The monotonic namespace is companion source. */

    /* The duration namespace: the printed form's parser alone. */
    {"Duration", "parse", SIG_FN(EK_ARROW_PURE,
                                  SIG_RESULT(SIG_STD("Duration")),
                                  SIG_STD("String"))},

    /* The interchange namespace: parsing only; show and its JSON
       mapping are derived services over the seven variants. */
    {"Json", "parse", SIG_FN(EK_ARROW_PURE, SIG_RESULT(SIG_STD("Json")),
                              SIG_STD("String"))},

    /* The path namespace over String: lexical path vocabulary, with
       no file-system contact. */
    {"Path", "join", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                             SIG_STD("String"), SIG_STD("String"))},
    {"Path", "normalize", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                  SIG_STD("String"))},
    {"Path", "is_absolute", SIG_FN(EK_ARROW_PURE, SIG_STD("Bool"),
                                    SIG_STD("String"))},

    /* The capability surface the source does not declare (Clause 10.8):
       the two advancing derivations, the error channel, the process
       exit, the four concurrency members, and the File family. `Never`
       is the bottom type, and the open arrows are the dispatchers. The
       capability-module source declares every other member (Clause
       9.1.1), so the namespaces are complete (Clause 7.9.1). The
       advancing derivations land on the std placeholders -- the same
       nominals the injected pairs wrap -- because the capability module
       declares no In or Rand type for the capability build to find.
       Constructing the error channel is pure: the writes through the
       value still carry the world bit of the Out members. */
    {"In", "from_lines", SIG_FN(EK_ARROW_PURE, SIG_STD("In"),
                                SIG_LIST(SIG_STD("String")))},
    {"Rand", "with_seed", SIG_FN(EK_ARROW_PURE, SIG_STD("Rand"),
                                 SIG_STD("Int"))},
    {"Out", "err", SIG_FN0(EK_ARROW_PURE, SIG_STD("Out"))},
    {"Proc", "exit", SIG_FN(EK_ARROW_WORLD, SIG_STD("Never"),
                            SIG_STD("Proc"), SIG_STD("Int"))},
    {"Conc", "scope", SIG_FN(EK_ARROW_OPEN, SIG_VAR(0), SIG_STD("Conc"),
                             SIG_FN(EK_ARROW_OPEN, SIG_VAR(0),
                                    SIG_SYS("Scope")))},
    {"Scope", "spawn", SIG_FN(EK_ARROW_OPEN, SIG_SYS1("Task", SIG_VAR(0)),
                              SIG_SYS("Scope"),
                              SIG_FN0(EK_ARROW_OPEN, SIG_VAR(0)))},
    {"Task", "join", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_VAR(0)),
                            SIG_SYS1("Task", SIG_VAR(0)))},
    {"Task", "cancel", SIG_FN(EK_ARROW_PURE, SIG_UNIT,
                              SIG_SYS1("Task", SIG_VAR(0)))},
    {"File", "read", SIG_FN(EK_ARROW_WORLD,
                            SIG_APP2("Result", SIG_STD("String"),
                                     SIG_SYS("IoError")),
                            SIG_SYS("File"), SIG_STD("Int"))},
    {"File", "read_bytes", SIG_FN(EK_ARROW_WORLD,
                                  SIG_APP2("Result", SIG_STD("Bytes"),
                                           SIG_SYS("IoError")),
                                  SIG_SYS("File"), SIG_STD("Int"))},
    {"File", "read_line", SIG_FN(EK_ARROW_WORLD,
                                 SIG_APP2("Result",
                                          SIG_OPTION(SIG_STD("String")),
                                          SIG_SYS("IoError")),
                                 SIG_SYS("File"))},
    {"File", "seek", SIG_FN(EK_ARROW_WORLD,
                            SIG_APP2("Result", SIG_UNIT,
                                     SIG_SYS("IoError")),
                            SIG_SYS("File"), SIG_STD("UInt"))},
    {"File", "write", SIG_FN(EK_ARROW_WORLD,
                             SIG_APP2("Result", SIG_UNIT,
                                      SIG_SYS("IoError")),
                             SIG_SYS("File"), SIG_STD("String"))},
    {"File", "write_bytes", SIG_FN(EK_ARROW_WORLD,
                                   SIG_APP2("Result", SIG_UNIT,
                                            SIG_SYS("IoError")),
                                   SIG_SYS("File"), SIG_STD("Bytes"))},
    {"File", "flush", SIG_FN(EK_ARROW_WORLD,
                             SIG_APP2("Result", SIG_UNIT,
                                      SIG_SYS("IoError")),
                             SIG_SYS("File"))},
    {"File", "close", SIG_FN(EK_ARROW_WORLD,
                             SIG_APP2("Result", SIG_UNIT,
                                      SIG_SYS("IoError")),
                             SIG_SYS("File"))},

    /* The unsigned scalar namespace: arithmetic without negation. */
    {"UInt", "add", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                           SIG_STD("UInt"), SIG_STD("UInt"))},
    {"UInt", "max", SIG_STD("UInt")},
    {"UInt", "mul", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                           SIG_STD("UInt"), SIG_STD("UInt"))},
    {"UInt", "quot", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                            SIG_STD("UInt"), SIG_STD("UInt"))},
    {"UInt", "rem", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                           SIG_STD("UInt"), SIG_STD("UInt"))},
    {"UInt", "sub", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                           SIG_STD("UInt"), SIG_STD("UInt"))},

    /* Conversions across domains, the parse family, and formatting. */
    {"UInt", "to_int", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("Int")),
                              SIG_STD("UInt"))},
    {"UInt", "to_float", SIG_FN(EK_ARROW_PURE, SIG_STD("Float"),
                               SIG_STD("UInt"))},
    {"UInt", "parse", SIG_FN(EK_ARROW_PURE, SIG_RESULT(SIG_STD("UInt")),
                            SIG_STD("String"))},
    {"UInt", "to_hex", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                             SIG_STD("UInt"))},
    {"UInt", "to_oct", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                             SIG_STD("UInt"))},
    {"UInt", "to_bin", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                             SIG_STD("UInt"))},
    {"UInt", "parse_hex", SIG_FN(EK_ARROW_PURE,
                                SIG_RESULT(SIG_STD("UInt")),
                                SIG_STD("String"))},
    {"UInt", "parse_oct", SIG_FN(EK_ARROW_PURE,
                                SIG_RESULT(SIG_STD("UInt")),
                                SIG_STD("String"))},
    {"UInt", "parse_bin", SIG_FN(EK_ARROW_PURE,
                                SIG_RESULT(SIG_STD("UInt")),
                                SIG_STD("String"))},

    /* The bitwise family over the unsigned ring. */
    {"UInt", "band", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                            SIG_STD("UInt"), SIG_STD("UInt"))},
    {"UInt", "bor", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                           SIG_STD("UInt"), SIG_STD("UInt"))},
    {"UInt", "bxor", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                            SIG_STD("UInt"), SIG_STD("UInt"))},
    {"UInt", "bnot", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                            SIG_STD("UInt"))},
    {"UInt", "shl", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                           SIG_STD("UInt"), SIG_STD("UInt"))},
    {"UInt", "shr", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                           SIG_STD("UInt"), SIG_STD("UInt"))},

    /* Wrapping arithmetic on the machine ring. */
    {"UInt", "wrapping_add", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                                   SIG_STD("UInt"), SIG_STD("UInt"))},
    {"UInt", "wrapping_sub", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                                   SIG_STD("UInt"), SIG_STD("UInt"))},
    {"UInt", "wrapping_mul", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                                   SIG_STD("UInt"), SIG_STD("UInt"))},
    {"UInt", "wrapping_neg", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                                   SIG_STD("UInt"))},

    /* The machine-word sizes: unsigned sources narrow to both signed
       and unsigned widths, and the float widths are total. */
    {"UInt", "to_i8", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("I8")),
                             SIG_STD("UInt"))},
    {"UInt", "to_i16", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("I16")),
                              SIG_STD("UInt"))},
    {"UInt", "to_i32", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("I32")),
                              SIG_STD("UInt"))},
    {"UInt", "to_i64", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("I64")),
                              SIG_STD("UInt"))},
    {"UInt", "to_u8", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("U8")),
                             SIG_STD("UInt"))},
    {"UInt", "to_u16", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("U16")),
                              SIG_STD("UInt"))},
    {"UInt", "to_u32", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("U32")),
                              SIG_STD("UInt"))},
    {"UInt", "to_u64", SIG_FN(EK_ARROW_PURE, SIG_OPTION(SIG_STD("U64")),
                              SIG_STD("UInt"))},
    {"UInt", "to_f32", SIG_FN(EK_ARROW_PURE, SIG_STD("F32"),
                              SIG_STD("UInt"))},
    {"UInt", "to_f64", SIG_FN(EK_ARROW_PURE, SIG_STD("F64"),
                              SIG_STD("UInt"))},
    {"UInt", "from_u8", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                               SIG_STD("U8"))},
    {"UInt", "from_u16", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                                SIG_STD("U16"))},
    {"UInt", "from_u32", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                                SIG_STD("U32"))},
    {"UInt", "from_u64", SIG_FN(EK_ARROW_PURE, SIG_STD("UInt"),
                                SIG_STD("U64"))},

    /* The machine words ferry, compare, show, and pad: the padding
       row is the only namespace member they carry; eq, the ordering
       family, and show derive. */
    {"I8", "to_hex_pad", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                SIG_STD("I8"), SIG_STD("UInt"))},
    {"I16", "to_hex_pad", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                 SIG_STD("I16"), SIG_STD("UInt"))},
    {"I32", "to_hex_pad", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                 SIG_STD("I32"), SIG_STD("UInt"))},
    {"I64", "to_hex_pad", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                 SIG_STD("I64"), SIG_STD("UInt"))},
    {"U8", "to_hex_pad", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                SIG_STD("U8"), SIG_STD("UInt"))},
    {"U16", "to_hex_pad", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                 SIG_STD("U16"), SIG_STD("UInt"))},
    {"U32", "to_hex_pad", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                 SIG_STD("U32"), SIG_STD("UInt"))},
    {"U64", "to_hex_pad", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                 SIG_STD("U64"), SIG_STD("UInt"))},
    {"F32", "to_hex_pad", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                 SIG_STD("F32"), SIG_STD("UInt"))},
    {"F64", "to_hex_pad", SIG_FN(EK_ARROW_PURE, SIG_STD("String"),
                                 SIG_STD("F64"), SIG_STD("UInt"))},

    /* A NULL name marks a namespace whose row set is complete: a
       name no station answers is absent. A partial namespace keeps
       the bridge for the names its rows do not carry yet. The
       capability namespaces are complete: the source members and the
       rows above carry every member of Annex C.9 (Clauses 7.9.1,
       10.8). */
    {"Args", NULL, NULL},
    {"Bool", NULL, NULL},
    {"Bytes", NULL, NULL},
    {"Char", NULL, NULL},
    {"Clock", NULL, NULL},
    {"Conc", NULL, NULL},
    {"Dict", NULL, NULL},
    {"Duration", NULL, NULL},
    {"Env", NULL, NULL},
    {"F32", NULL, NULL},
    {"F64", NULL, NULL},
    {"File", NULL, NULL},
    {"Float", NULL, NULL},
    {"Fs", NULL, NULL},
    {"I16", NULL, NULL},
    {"I32", NULL, NULL},
    {"I64", NULL, NULL},
    {"I8", NULL, NULL},
    {"In", NULL, NULL},
    {"Int", NULL, NULL},
    {"Json", NULL, NULL},
    {"List", NULL, NULL},
    {"Mono", NULL, NULL},
    {"Option", NULL, NULL},
    {"Out", NULL, NULL},
    {"Path", NULL, NULL},
    {"Proc", NULL, NULL},
    {"Rand", NULL, NULL},
    {"Range", NULL, NULL},
    {"Result", NULL, NULL},
    {"Scope", NULL, NULL},
    {"Set", NULL, NULL},
    {"String", NULL, NULL},
    {"Task", NULL, NULL},
    {"Time", NULL, NULL},
    {"U16", NULL, NULL},
    {"U32", NULL, NULL},
    {"U64", NULL, NULL},
    {"U8", NULL, NULL},
    {"UInt", NULL, NULL},
};

/* The table's length, for the consult loops. */
const size_t ck_ns_rows_len = sizeof ck_ns_rows / sizeof ck_ns_rows[0];


/* The injected pairs of the capability module (Clause 10.8): the
   compiler rows behind the `_`-prefixed names the cap source
   references without declaring. Each pair is the unwrap and wrap of
   one session bundle. Their rows carry no declaration node, so the
   schemes hang here and the pair stays unnameable outside the
   capability module. The file bundle is generic over the callback
   result of `with_file`; its variable generalizes, so every use
   instantiates its own. */
const ns_pair_row ck_ns_pair_rows[] = {
    {"_out_unwrap", SIG_FN(EK_ARROW_PURE, SIG_CAP("_OutB"),
                           SIG_STD("Out"))},
    {"_out_wrap", SIG_FN(EK_ARROW_PURE, SIG_STD("Out"),
                         SIG_CAP("_OutB"))},
    {"_in_unwrap", SIG_FN(EK_ARROW_PURE, SIG_CAP("_InB"),
                          SIG_STD("In"))},
    {"_in_wrap", SIG_FN(EK_ARROW_PURE, SIG_STD("In"),
                        SIG_CAP("_InB"))},
    {"_fs_unwrap", SIG_FN(EK_ARROW_PURE, SIG_CAP1("_FsB", SIG_VAR(0)),
                          SIG_STD("Fs"))},
    {"_fs_wrap", SIG_FN(EK_ARROW_PURE, SIG_STD("Fs"),
                        SIG_CAP1("_FsB", SIG_VAR(0)))},
    {"_env_unwrap", SIG_FN(EK_ARROW_PURE, SIG_CAP("_EnvB"),
                           SIG_STD("Env"))},
    {"_env_wrap", SIG_FN(EK_ARROW_PURE, SIG_STD("Env"),
                         SIG_CAP("_EnvB"))},
    {"_args_unwrap", SIG_FN(EK_ARROW_PURE, SIG_CAP("_ArgsB"),
                            SIG_STD("Args"))},
    {"_args_wrap", SIG_FN(EK_ARROW_PURE, SIG_STD("Args"),
                          SIG_CAP("_ArgsB"))},
    {"_clock_unwrap", SIG_FN(EK_ARROW_PURE, SIG_CAP("_ClockB"),
                             SIG_STD("Clock"))},
    {"_clock_wrap", SIG_FN(EK_ARROW_PURE, SIG_STD("Clock"),
                           SIG_CAP("_ClockB"))},
    {"_rand_unwrap", SIG_FN(EK_ARROW_PURE, SIG_CAP("_RandB"),
                            SIG_STD("Rand"))},
    {"_rand_wrap", SIG_FN(EK_ARROW_PURE, SIG_STD("Rand"),
                          SIG_CAP("_RandB"))},
};

/* The pair table's length. */
const size_t ck_ns_pair_rows_len
    = sizeof ck_ns_pair_rows / sizeof ck_ns_pair_rows[0];
