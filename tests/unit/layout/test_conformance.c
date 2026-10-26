#include "common.h"

#include <stdio.h>
#include <stdlib.h>

/* Smoke test: every conformance module (Annex D sources, kept under
   tests/conformance) must lex and lay out without a single
   error diagnostic. */

static void layout_module(const char *path)
{
    size_t len = 0;
    char *data = fx_read_file(path, &len);
    layout_fixture f;

    EK_CHECK(data != NULL);
    if (!data) {
        return;
    }
    fx_begin(&f, data);
    EK_CHECK_U64(ek_diag_count(f.diag, EK_SEV_ERROR), 0);
    EK_CHECK_U64(ek_diag_count(f.diag, EK_SEV_WARNING), 0);
    EK_CHECK(fx_count(&f) > 0);
    EK_CHECK_U64(fx_tok(&f, fx_count(&f) - 1).kind, EK_LTOK_EOF);
    printf("%s: %llu layout tokens\n", path,
           (unsigned long long)fx_count(&f));
    fx_end(&f);
    free(data);
}

int main(void)
{
    size_t i, count = 0;
    const char *const *mods = fx_modules(&count);

    for (i = 0; i < count; i++) {
        layout_module(mods[i]);
    }
    return ek_test_report("layout/conformance");
}
