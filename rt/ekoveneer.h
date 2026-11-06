#ifndef EK_RT_EKOVENEER_H
#define EK_RT_EKOVENEER_H

#include <stddef.h>
#include <stdint.h>

/* The embedding veneer's entry points. */

typedef struct eko_opaque *eko_value;
typedef struct eko_module_opaque *eko_module;

/* Compiles and loads one module file. The path is borrowed
   NUL-terminated UTF-8; the result is NULL when the module does not
   compile or load. */
eko_module *eko_eval_file(const char *path);

/* One exported binding of a module by its public name. The name is
   borrowed NUL-terminated UTF-8; the result is NULL when the module
   exports no binding of that name. */
eko_value eko_get_export(const eko_module *module, const char *name);

/* One call over an argument vector. The arguments are borrowed for
   the call alone; the result arrives at no reference of its own. */
eko_value eko_call(eko_value fn, const eko_value *args, size_t n);

/* The hold protocol over a value the host received. */
void eko_retain(eko_value v);
void eko_release(eko_value v);

/* The string bridge: `eko_string_new` copies its NUL-terminated UTF-8
   bytes; `eko_as_string` lends the value's own bytes, which stay
   valid for as long as the host holds the value, and is NULL when
   the value is not a string. */
eko_value eko_string_new(const char *utf8);
const char *eko_as_string(eko_value v);

/* The scalar bridge. */
eko_value eko_int_new(int64_t i);
int64_t eko_as_int(eko_value v);
eko_value eko_float_new(double f);
double eko_as_float(eko_value v);
eko_value eko_bool_new(int b);
int eko_as_bool(eko_value v);
eko_value eko_unit_new(void);

/* A C function as a closure value. The closure manages no part of
   `ctx`: the host keeps `ctx` valid for the closure's lifetime. */
typedef eko_value (*eko_fn)(void *ctx, const eko_value *args, size_t n);
eko_value eko_closure_new(eko_fn fn, void *ctx);

/* The capability constructors, one per capability, each mirroring its
   bundle record (Clause 10.8); the Out constructor is the named
   example. The write closure receives the text borrowed, as
   NUL-terminated UTF-8. */
typedef void (*eko_write)(void *ctx, const char *utf8);
eko_value eko_cap_out(eko_write write, void *ctx);

/* A best-effort observer at abort; it is not a recovery mechanism
   (Clause 10.7.5). */
typedef void (*eko_panic_observer)(const char *code, const char *message);
void eko_on_panic(eko_panic_observer observer);

#endif
