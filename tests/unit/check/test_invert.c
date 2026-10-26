#include "common.h"

/* Inverted bindings: the implicit callback occupies the dispatcher's
   last parameter, the pattern binds the callback's parameters, and the
   remaining block is the callback body. */

/* The first row of one kind on a source line. */
static const ek_ck_row *row_of_kind(const cfx *f, int line,
                                    ek_node_kind kind)
{
    size_t i;

    for (i = 0; i < f->chk.count; i++) {
        if (f->chk.rows[i].node && f->chk.rows[i].node->line == line
            && f->chk.rows[i].node->kind == kind) {
            return &f->chk.rows[i];
        }
    }
    return NULL;
}

/* Any call row on the line carries the bit. */
static int call_bit_at(const cfx *f, int line, ek_bit bit)
{
    size_t i;

    for (i = 0; i < f->chk.count; i++) {
        if (f->chk.rows[i].node && f->chk.rows[i].node->line == line
            && f->chk.rows[i].node->kind == EK_N_CALL
            && f->chk.rows[i].bit == bit) {
            return 1;
        }
    }
    return 0;
}

/* A callback in the last position is supplied implicitly, and the
   continuation is checked at the callback's result type. */
static void user_dispatcher(void)
{
    cfx f;
    const ek_ck_row *call;

    cfx_begin(&f,
              "apply: (n: Int, cb: (Int) -> Int) -> Int = cb(n)\n"
              "g: (n: Int) -> Int =\n"
              "    x <- apply(n)\n"
              "    x + 1\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    call = row_of_kind(&f, 3, EK_N_CALL);
    EK_CHECK(call != NULL && call->type
             && call->type->kind == EK_T_NOMINAL
             && strcmp(call->type->u.nominal.sym->name, "Int") == 0);
    cfx_end(&f);

    /* The zero-pattern form passes a zero-parameter callback. */
    cfx_begin(&f,
              "f: (cb: () -> Int) -> Int = cb()\n"
              "g: () -> Int =\n"
              "    <- f()\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* A parameter group binds one callback parameter each. */
    cfx_begin(&f,
              "each2: (cb: (Int, Int) -> Int) -> Int = cb(1, 2)\n"
              "g: () -> Int =\n"
              "    (a, b) <- each2()\n"
              "    a + b\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

/* The continuation is the callback body, so its type is the callback
   result and its calls belong to the dispatcher call. */
static void continuation_body(void)
{
    cfx f;

    cfx_begin(&f,
              "f: (cb: () -> Int) -> Int = cb()\n"
              "g: () -> Int =\n"
              "    <- f()\n"
              "    \"s\"\n");
    EK_CHECK(cfx_has_code(&f, "E-nominal-mismatch"));
    EK_CHECK(cfx_code_at_line(&f, "E-nominal-mismatch", 4));
    cfx_end(&f);

    cfx_begin(&f,
              "f: () -> Result Int String = (): Ok 1\n"
              "save: (out: Out) !-> Result _ String =\n"
              "    x <- f().try()\n"
              "    Out.println(out, show(x))\n"
              "    Ok _\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(call_bit_at(&f, 3, EK_BIT_WORLD));
    cfx_end(&f);
}

/* A callback in a non-final parameter position has no trailing form,
   so the implicit callback makes the arity too large. */
static void callback_not_last(void)
{
    cfx f;

    cfx_begin(&f,
              "f: (cb: (Int) -> Int, n: Int) -> Int = cb(n)\n"
              "g: (n: Int) -> Int =\n"
              "    x <- f(n)\n"
              "    x\n");
    EK_CHECK(cfx_has_code(&f, "E-type-mismatch"));
    cfx_end(&f);
}

int main(void)
{
    user_dispatcher();
    continuation_body();
    callback_not_last();
    return ek_test_report("check/invert");
}
