#include "internal.h"

#include <stdio.h>

/* The In channel: the standard input's bundle and the advancing
   line-list derivation. Both build the session-read bundle, one
   closure over the read_line operation, and both report their results
   through the emitted unit's Result, Option, and IoError identities:
   end of input is Ok None, not an error. */

static ek_val read_ok_some(ek_obj *line)
{
    ek_val s = ek_val_ref(line);
    ek_val some = ek_gen_option_some(s);
    ek_val r;

    ek_release(s);
    r = ek_gen_result_ok(some);
    ek_release(some);
    return r;
}

static ek_val read_ok_none(void)
{
    ek_val none = ek_gen_option_none();
    ek_val r = ek_gen_result_ok(none);

    ek_release(none);
    return r;
}

static ek_val read_err(int kind, const char *msg)
{
    ek_val ioerr = ek_gen_ioerr(kind, msg);
    ek_val r = ek_gen_result_err(ioerr);

    ek_release(ioerr);
    return r;
}

/* The stdin read: one line per call, end of input at the stream's end,
   and InvalidData for bytes that are not well-formed UTF-8. */
static ek_val in_read_line(ek_val env, ek_val *args, size_t argc)
{
    ek_obj *line;
    int eof = 0, bad = 0;

    (void)env;
    (void)args;
    if (argc != 0) {
        return read_ok_none();
    }
    line = ek_line_read(stdin, &eof, &bad);
    if (bad) {
        return read_err(EK_IO_INVALID_DATA,
                        "invalid UTF-8 on standard input");
    }
    if (eof) {
        return read_ok_none();
    }
    return read_ok_some(line);
}

ek_val ek_cap_in_stdin(void)
{
    return ek_bundle1_new(ek_closure_new(in_read_line, ek_val_unit()));
}

/* The line-list derivation: the closure's environment carries the list
   and the cursor, so the sequence advances across calls and stays at
   end of input afterwards. */
typedef struct {
    ek_obj obj;
    ek_val lines;
    size_t cursor;
} ek_lines_cell;

static void lines_cell_drop(ek_obj *obj)
{
    ek_release(((ek_lines_cell *)obj)->lines);
}

static const ek_typeinfo ek_typeinfo_lines_cell = { EK_KIND_STRUCT,
                                                    lines_cell_drop };

/* The element at the cursor, borrowed; NULL past the end. */
static ek_obj *cell_line(ek_lines_cell *cell)
{
    ek_val list = cell->lines;
    size_t left = cell->cursor;

    while (left > 0 && ek_val_is_cons(list)) {
        list = ek_cons_tail(list.as.ref);
        left--;
    }
    if (!ek_val_is_cons(list)) {
        return NULL;
    }
    return ek_cons_head(list.as.ref).as.ref;
}

static ek_val lines_read_line(ek_val env, ek_val *args, size_t argc)
{
    ek_lines_cell *cell;
    ek_obj *line;

    (void)args;
    if (argc != 0 || !ek_val_is_ref(env)
        || env.as.ref->info != &ek_typeinfo_lines_cell) {
        return read_ok_none();
    }
    cell = (ek_lines_cell *)env.as.ref;
    line = cell_line(cell);
    if (line == NULL) {
        return read_ok_none();
    }
    cell->cursor++;
    return read_ok_some(line);
}

/* Builds the In bundle over the lines: the caller keeps its own
   reference, the cell retains the list. */
ek_val ek_in_from_lines(ek_obj *lines)
{
    ek_lines_cell *cell = ek_rt_alloc_zeroed(sizeof *cell);
    ek_val env;
    ek_val bundle;

    cell->obj.info = &ek_typeinfo_lines_cell;
    cell->obj.rc = 1;
    cell->lines = ek_val_ref(lines);
    ek_retain(cell->lines);
    env = ek_val_ref(&cell->obj);
    bundle = ek_bundle1_new(ek_closure_new(lines_read_line, env));
    ek_release(env);
    return bundle;
}
