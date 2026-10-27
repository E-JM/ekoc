#include "common.h"

/* Documentation comments: `///` yields a DOC token carrying the line's
   text, while `//` stays skipped. The token is not a significant
   token, so the rest of the stream keeps its shape. */

static void test_doc_text(void)
{
    lex_fixture f;

    fx_begin(&f, "/// hello");
    EK_CHECK_U64(fx_count(&f), 2);
    EK_CHECK_U64(fx_tok(&f, 0).kind, EK_TOK_DOC);
    EK_CHECK_U64(fx_tok(&f, 0).line, 1);
    EK_CHECK_U64(fx_tok(&f, 0).col, 1);
    EK_CHECK_STR(fx_tok(&f, 0).text, "hello");
    EK_CHECK_U64(fx_tok(&f, 0).len, 5);
    EK_CHECK_U64(fx_errors(&f), 0);
    fx_end(&f);

    /* One leading space is the separator; the rest is content. */
    fx_begin(&f, "///  two");
    EK_CHECK_STR(fx_tok(&f, 0).text, " two");
    EK_CHECK_U64(fx_tok(&f, 0).len, 4);
    fx_end(&f);

    fx_begin(&f, "///");
    EK_CHECK_STR(fx_tok(&f, 0).text, "");
    EK_CHECK_U64(fx_tok(&f, 0).len, 0);
    fx_end(&f);

    /* A tab after the opener is content, not E-tab. */
    fx_begin(&f, "///\tcontent");
    EK_CHECK_STR(fx_tok(&f, 0).text, "\tcontent");
    EK_CHECK_U64(fx_errors(&f), 0);
    fx_end(&f);
}

static void test_slash_run(void)
{
    lex_fixture f;

    /* `///` wins over `//`: the fourth slash starts the text. */
    fx_begin(&f, "////");
    EK_CHECK_U64(fx_count(&f), 2);
    EK_CHECK_U64(fx_tok(&f, 0).kind, EK_TOK_DOC);
    EK_CHECK_STR(fx_tok(&f, 0).text, "/");
    fx_end(&f);

    fx_begin(&f, "/////");
    EK_CHECK_STR(fx_tok(&f, 0).text, "//");
    fx_end(&f);

    /* A plain line comment emits nothing. */
    fx_begin(&f, "// plain");
    EK_CHECK_U64(fx_count(&f), 1);
    EK_CHECK_U64(fx_tok(&f, 0).kind, EK_TOK_EOF);
    fx_end(&f);

    /* A trailing document keeps the line's significant tokens. */
    fx_begin(&f, "x /// doc");
    EK_CHECK_U64(fx_count(&f), 3);
    EK_CHECK_U64(fx_tok(&f, 0).kind, EK_TOK_IDENT);
    EK_CHECK_U64(fx_tok(&f, 1).kind, EK_TOK_DOC);
    EK_CHECK_STR(fx_tok(&f, 1).text, "doc");
    EK_CHECK_U64(fx_errors(&f), 0);
    fx_end(&f);
}

static void test_outside_comments(void)
{
    lex_fixture f;

    /* Inside a string the slashes are text. */
    fx_begin(&f, "\"/// text\"");
    EK_CHECK_U64(fx_count(&f), 2);
    EK_CHECK_U64(fx_tok(&f, 0).kind, EK_TOK_STRING);
    fx_end(&f);

    /* Inside a bracket the document is still a comment token. */
    fx_begin(&f, "f(1, /// doc\n2)");
    EK_CHECK_U64(fx_count(&f), 9);
    EK_CHECK_U64(fx_tok(&f, 4).kind, EK_TOK_DOC);
    EK_CHECK_STR(fx_tok(&f, 4).text, "doc");
    EK_CHECK_U64(fx_tok(&f, 5).kind, EK_TOK_NEWLINE);
    EK_CHECK_U64(fx_errors(&f), 0);
    fx_end(&f);
}

int main(void)
{
    test_doc_text();
    test_slash_run();
    test_outside_comments();
    return ek_test_report("lex/docs");
}
