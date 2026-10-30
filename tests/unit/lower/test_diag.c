#include "common.h"

/* The E-lower diagnostics: constructs outside the lowerer's coverage
   come back as errors at the construct's position, and the parts that
   lowered still produce text. The derived services and the
   string-keyed dictionaries, once gaps, now lower. */

static void derived_service(void)
{
    lfx f;

    lfx_begin(&f, "main: () -> _ =\n"
                  "    s = show(7)\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK_U64(f.low.fn_count, 1);
    EK_CHECK(lfx_has(&f, "ekd_show_std_Int("));
    EK_CHECK(f.low.text != NULL);
    EK_CHECK(lfx_has(&f, "#include \"ekort.h\""));
    EK_CHECK(lfx_has(&f, "int main(void)"));
    lfx_end(&f);
}

static void dict_string_key(void)
{
    lfx f;

    lfx_begin(&f, "look: (d: [String]Int) -> Int = 0\n"
                  "main: () -> _ =\n"
                  "    d: [String]Int = {\"ada\": 36}\n"
                  "    n = look(d)\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK_U64(f.low.fn_count, 2);
    /* the key hashes its bytes, not its address */
    EK_CHECK(lfx_has(&f, "ek_dict_set("));
    EK_CHECK(f.low.text != NULL);
    lfx_end(&f);
}

static void interpolated_string(void)
{
    lfx f;

    lfx_begin(&f, "main: () -> _ =\n"
                  "    s = \"a{1}\"\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_has_code(&f, "E-lower"));
    EK_CHECK(lfx_code_at_line(&f, "E-lower", 2));
    EK_CHECK_U64(lfx_errors(&f), 1);
    EK_CHECK(f.low.text != NULL);
    lfx_end(&f);
}

int main(void)
{
    derived_service();
    dict_string_key();
    interpolated_string();
    return ek_test_report("lower/diag");
}
