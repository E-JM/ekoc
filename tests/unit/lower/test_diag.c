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
    EK_CHECK(lfx_has(&f, "ekd_show_Int("));
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

/* Interpolation lowers; a function slot renders through its own
   display service now that the registry backs one. */
static void interpolated_string(void)
{
    lfx f;

    lfx_begin(&f, "main: () -> _ =\n"
                  "    h: (Int) -> Int = (x): x\n"
                  "    s = \"a{h}\"\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "static ek_obj *ekd_show_Fn(ek_val v);"));
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
