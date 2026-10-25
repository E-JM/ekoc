#include <string.h>

#include "harness.h"

#include "lex/internal.h"

/* The progress guard of the main loop: a scanner step that consumes
   nothing must cost one E-lex-stall diagnostic and one skipped byte,
   never a hang. White-box on purpose; the guard only fires on a latent
   scanner bug, so the suite feeds one to it through the injected
   scanner. A regression like the zero-length `0x` literal would have
   surfaced here as a stall diagnostic instead of a hung runner. */

static int stall(lex_ctx *c, gbuf *out)
{
    (void)c;
    (void)out;
    return 1; /* the failure mode the guard exists for */
}

static int advance_only(lex_ctx *c, gbuf *out)
{
    (void)out;
    c->pos++; /* progress without a token is not a stall */
    c->col++;
    return 1;
}

static size_t count_code(ek_diag *diag, const char *code)
{
    size_t n = 0, i, hits = 0;
    const ek_diagnostic *ds = ek_diag_drain(diag, &n);

    for (i = 0; i < n; i++) {
        if (ds[i].code && strcmp(ds[i].code, code) == 0) {
            hits++;
        }
    }
    return hits;
}

static void make_ctx(lex_ctx *c, ek_arena *arena, ek_diag *diag)
{
    memset(c, 0, sizeof *c);
    c->src = "0x";
    c->len = 2;
    c->line = 1;
    c->col = 1;
    c->file = "test.eko";
    c->arena = arena;
    c->diag = diag;
}

int main(void)
{
    ek_arena *arena = ek_arena_create();
    ek_diag *diag = ek_diag_create();

    {
        lex_ctx c;
        gbuf out;

        make_ctx(&c, arena, diag);
        gbuf_init(&out, arena);
        EK_CHECK(lex_step(&c, &out, stall) == 1);
        EK_CHECK_U64(c.pos, 1); /* exactly one byte of progress */
        EK_CHECK_U64(c.col, 2);
        EK_CHECK(lex_step(&c, &out, stall) == 1); /* still moving */
        EK_CHECK_U64(c.pos, 2);
        /* the guard itself is unbounded: callers gate on end of input */
        EK_CHECK(lex_step(&c, &out, stall) == 1);
        EK_CHECK_U64(c.pos, 3);
        EK_CHECK_U64(count_code(diag, "E-lex-stall"), 3);
    }
    {
        lex_ctx c;
        gbuf out;

        make_ctx(&c, arena, diag);
        gbuf_init(&out, arena);
        EK_CHECK(lex_step(&c, &out, advance_only) == 1);
        EK_CHECK_U64(c.pos, 1);
        EK_CHECK_U64(ek_diag_count(diag, EK_SEV_ERROR), 0);
    }
    ek_diag_destroy(diag);
    ek_arena_destroy(arena);
    return ek_test_report("lex/progress");
}
