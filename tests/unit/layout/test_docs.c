#include "common.h"

/* Documentation passthrough: a run of documentation lines rides before
   the binding it attaches to, after the binding line's layout markers,
   and never affects the line shape. */

static size_t doc_count(const layout_fixture *f)
{
    size_t i, n = 0;

    for (i = 0; i < fx_count(f); i++) {
        if (fx_tok(f, i).kind == EK_LTOK_SOURCE
            && fx_tok(f, i).tok.kind == EK_TOK_DOC) {
            n++;
        }
    }
    return n;
}

static const ek_layout_token *first_doc(const layout_fixture *f)
{
    size_t i;

    for (i = 0; i < fx_count(f); i++) {
        if (fx_tok(f, i).kind == EK_LTOK_SOURCE
            && fx_tok(f, i).tok.kind == EK_TOK_DOC) {
            return &f->r.tokens[i];
        }
    }
    return NULL;
}

static void test_top_level_document(void)
{
    layout_fixture f;
    char *got;
    const ek_layout_token *doc;

    fx_begin(&f, "a = 1\n/// one\n/// two\nb = 2\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    EK_CHECK_U64(doc_count(&f), 2);
    doc = first_doc(&f);
    EK_CHECK(doc != NULL);
    if (doc) {
        EK_CHECK_U64(doc->line, 2);
        EK_CHECK_STR(doc->tok.text, "one");
    }
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S S N S S S S S N E");
    free(got);
    fx_end(&f);

    /* A literal-headed pattern binding line attaches its document. */
    fx_begin(&f, "/// which arm\n\"timeout\" = s\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    EK_CHECK_U64(doc_count(&f), 1);
    doc = first_doc(&f);
    EK_CHECK(doc != NULL);
    if (doc) {
        EK_CHECK_STR(doc->tok.text, "which arm");
    }
    fx_end(&f);

    fx_begin(&f, "/// the negative one\n-1 = n\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    EK_CHECK_U64(doc_count(&f), 1);
    fx_end(&f);
}

static void test_block_document(void)
{
    layout_fixture f;
    char *got;
    size_t i, indent = fx_count(&f);

    fx_begin(&f, "f =\n    /// doc\n    x = 1\n    x\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    EK_CHECK_U64(doc_count(&f), 1);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S N I S S S S N S N D E");
    free(got);
    /* INDENT comes before the document, which comes before the member. */
    for (i = 0; i < fx_count(&f); i++) {
        if (fx_tok(&f, i).kind == EK_LTOK_INDENT) {
            indent = i;
            break;
        }
    }
    EK_CHECK(indent + 2 < fx_count(&f));
    if (indent + 2 < fx_count(&f)) {
        EK_CHECK(fx_tok(&f, indent + 1).kind == EK_LTOK_SOURCE);
        EK_CHECK(fx_tok(&f, indent + 1).tok.kind == EK_TOK_DOC);
        EK_CHECK(fx_tok(&f, indent + 2).kind == EK_LTOK_SOURCE);
        EK_CHECK_STR(fx_tok(&f, indent + 2).tok.text, "x");
    }
    fx_end(&f);
}

static void test_document_break(void)
{
    layout_fixture f;
    char *got;

    /* A blank line ends an unattached document. */
    fx_begin(&f, "/// doc\n\nx = 1\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    EK_CHECK_U64(doc_count(&f), 0);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S S N E");
    free(got);
    fx_end(&f);

    /* So does an ordinary comment line. */
    fx_begin(&f, "/// doc\n// note\nx = 1\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    EK_CHECK_U64(doc_count(&f), 0);
    fx_end(&f);
}

static void test_document_before_non_binding(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f, "/// doc\nif x: 1\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    EK_CHECK_U64(doc_count(&f), 0);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S S S N E");
    free(got);
    fx_end(&f);
}

static void test_trailing_document(void)
{
    layout_fixture f;
    char *got;
    const ek_layout_token *doc;

    /* A trailing document belongs to the following line, not the line it
       shares. */
    fx_begin(&f, "x = 1 /// doc\ny = 2\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    EK_CHECK_U64(doc_count(&f), 1);
    doc = first_doc(&f);
    EK_CHECK(doc != NULL);
    if (doc) {
        EK_CHECK_U64(doc->line, 1);
    }
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S S N S S S S N E");
    free(got);
    fx_end(&f);
}

static void test_document_after_dedent(void)
{
    layout_fixture f;
    size_t i;
    int seen_dedent = 0, seen_doc = 0;

    fx_begin(&f, "f =\n    x = 1\n/// doc\ny = 2\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    EK_CHECK_U64(doc_count(&f), 1);
    for (i = 0; i < fx_count(&f); i++) {
        const ek_layout_token *t = &f.r.tokens[i];

        if (t->kind == EK_LTOK_DEDENT) {
            seen_dedent = 1;
        } else if (t->kind == EK_LTOK_SOURCE
                   && t->tok.kind == EK_TOK_DOC) {
            EK_CHECK(seen_dedent);
            seen_doc = 1;
        } else if (seen_doc && t->kind == EK_LTOK_SOURCE
                   && t->tok.kind == EK_TOK_IDENT) {
            EK_CHECK_STR(t->tok.text, "y");
            break;
        }
    }
    EK_CHECK(seen_dedent);
    EK_CHECK(seen_doc);
    fx_end(&f);
}

int main(void)
{
    test_top_level_document();
    test_block_document();
    test_document_break();
    test_document_before_non_binding();
    test_trailing_document();
    test_document_after_dedent();
    return ek_test_report("layout/docs");
}
