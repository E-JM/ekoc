#include "common.h"

/* The interpolation slots and the range family at the text level: a
   multi-part literal lowers to the escaped segments and the slots'
   show services joined by the string concatenation, the two range
   operators build the generated struct family of their element slot,
   and the namespace members route to the runtime's traversal
   entries. */

/* The segments stay verbatim and each slot renders through its row
   type's show. */
static void multi_segment(void)
{
    lfx f;

    lfx_begin(&f, "main: () -> _ =\n"
                  "    n = 3\n"
                  "    s = \"ada\"\n"
                  "    t = \"n = {n} and s = {s}\"\n"
                  "    _ := t\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ek_str_from_cstr(\"n = \")"));
    EK_CHECK(lfx_has(&f, "ek_str_from_cstr(\" and s = \")"));
    EK_CHECK(lfx_has(&f, "ekd_show_std_Int("));
    EK_CHECK(lfx_has(&f, "ekd_show_std_String("));
    EK_CHECK(lfx_has(&f, "ek_str_concat("));
    EK_CHECK(lfx_has(&f, "ek_release_obj("));
    EK_CHECK_U64(lfx_errors(&f), 0);
    lfx_end(&f);
}

/* A literal with one slot and no literal parts is the show call
   alone; the string channel renders a String slot bare. */
static void single_slot(void)
{
    lfx f;

    lfx_begin(&f, "main: () -> _ =\n"
                  "    n = 7\n"
                  "    t = \"{n}\"\n"
                  "    _ := t\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ekd_show_std_Int("));
    EK_CHECK(lfx_lacks(&f, "ek_str_concat("));
    EK_CHECK_U64(lfx_errors(&f), 0);
    lfx_end(&f);
}

/* The escaped brace is a literal segment, not a slot. */
static void escaped_brace(void)
{
    lfx f;

    lfx_begin(&f, "main: () -> _ =\n"
                  "    t = \"set \\{ is literal\"\n"
                  "    _ := t\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ek_str_from_cstr(\"set { is literal\")"));
    EK_CHECK(lfx_lacks(&f, "ek_str_concat("));
    EK_CHECK_U64(lfx_errors(&f), 0);
    lfx_end(&f);
}

/* A slot may hold a nested string literal with its own slot, and a
   composite renders through its own derived show. */
static void nested_and_composite(void)
{
    lfx f;

    lfx_begin(&f, "Money: type = [cents: Int, cur: String]\n"
                  "main: () -> _ =\n"
                  "    n = 3\n"
                  "    m: Money = {cents: 12500, cur: \"USD\"}\n"
                  "    a = \"outer {\"inner {n}\"}\"\n"
                  "    b = \"{m}\"\n"
                  "    _ := a\n"
                  "    _ := b\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ek_str_from_cstr(\"outer \")"));
    EK_CHECK(lfx_has(&f, "ek_str_from_cstr(\"inner \")"));
    EK_CHECK(lfx_has(&f, "ekd_show_test_Money("));
    EK_CHECK_U64(lfx_errors(&f), 0);
    lfx_end(&f);
}

/* The two operator forms build the family over the element slot, the
   closed flag the only difference; a Char range never shares the Int
   layout. */
static void range_construction(void)
{
    lfx f;

    lfx_begin(&f, "main: () -> _ =\n"
                  "    a = 1..9\n"
                  "    b = 1..=9\n"
                  "    c = 'a'..'e'\n"
                  "    _ := a\n"
                  "    _ := b\n"
                  "    _ := c\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "typedef struct { ek_obj obj; ek_val f_start;"
                         " ek_val f_end; ek_val f_closed; }"
                         " ekS_std_Range_Int;"));
    EK_CHECK(lfx_has(&f, "ekn_std_Range_Int(t0, t1, ek_bool(0))"));
    EK_CHECK(lfx_has(&f, "ek_bool(1)"));
    EK_CHECK(lfx_has(&f, "ekS_std_Range_Char"));
    EK_CHECK_U64(lfx_errors(&f), 0);
    lfx_end(&f);
}

/* An unsigned element slot keys its own family. */
static void uint_range(void)
{
    lfx f;

    lfx_begin(&f, "main: () -> _ =\n"
                  "    a: UInt = 1\n"
                  "    b: UInt = 4\n"
                  "    r = a..=b\n"
                  "    _ := r\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ekn_std_Range_UInt("));
    EK_CHECK_U64(lfx_errors(&f), 0);
    lfx_end(&f);
}

/* The namespace rows route to the runtime entries, the receiver
   first and the written arguments after it. */
static void range_members(void)
{
    lfx f;

    lfx_begin(&f, "use: (r: Range Int) -> Int =\n"
                  "    n = r.length()\n"
                  "    xs = r.to_list()\n"
                  "    ys = r.map((v): v * 2)\n"
                  "    z = r.fold(0, (acc, v): acc + v)\n"
                  "    r.each((v): _)\n"
                  "    r.each_with_index((v, i): _)\n"
                  "    q = n + z + xs.length() + ys.length()\n"
                  "    q\n"
                  "main: () -> _ =\n"
                  "    w = use(1..=9)\n"
                  "    _ := w\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ek_range_length("));
    EK_CHECK(lfx_has(&f, "ek_range_to_list("));
    EK_CHECK(lfx_has(&f, "ek_range_map("));
    EK_CHECK(lfx_has(&f, "ek_range_fold("));
    EK_CHECK(lfx_has(&f, "ek_range_each("));
    EK_CHECK(lfx_has(&f, "ek_range_each_index("));
    EK_CHECK_U64(lfx_errors(&f), 0);
    lfx_end(&f);
}

/* The display and comparison services specialize over the applied
   range type. */
static void range_services(void)
{
    lfx f;

    lfx_begin(&f, "main: () -> _ =\n"
                  "    a = 1..9\n"
                  "    b = 1..=9\n"
                  "    s = a.show()\n"
                  "    e = a.eq(b)\n"
                  "    o = a.lt(b)\n"
                  "    _ := s\n"
                  "    _ := e\n"
                  "    _ := o\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ekd_show_std_Range_std_Int("));
    EK_CHECK(lfx_has(&f, "ekq_std_Range_std_Int("));
    EK_CHECK(lfx_has(&f, "eko_std_Range_std_Int("));
    EK_CHECK(lfx_has(&f, "f_closed.as.i"));
    EK_CHECK_U64(lfx_errors(&f), 0);
    lfx_end(&f);
}

int main(void)
{
    multi_segment();
    single_slot();
    escaped_brace();
    nested_and_composite();
    range_construction();
    uint_range();
    range_members();
    range_services();
    return ek_test_report("lower/interp");
}
