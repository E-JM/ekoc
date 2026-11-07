#include "common.h"

/* The new dispatch rows: the set vocabulary, the radix and padded
   display channels, the dictionary traversals, and the Json document
   reader whose routing stamps the runtime-shared identities exactly
   once. */

static void set_rows_route(void)
{
    lfx f;

    lfx_begin(&f,
              "main: () -> _ =\n"
              "    s = Set.of({3, 1})\n"
              "    s9 = s.insert(9)\n"
              "    sr = s9.remove(3)\n"
              "    has = s.contains(2)\n"
              "    n = s.length()\n"
              "    xs = s.to_list()\n"
              "    _ := sr\n"
              "    _ := has\n"
              "    _ := n\n"
              "    _ := xs\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ek_set_of("));
    EK_CHECK(lfx_has(&f, "ek_set_insert("));
    EK_CHECK(lfx_has(&f, "ek_set_remove("));
    /* contains answers Bool over the C truth word */
    EK_CHECK(lfx_has(&f, "ek_bool(ek_set_contains("));
    EK_CHECK(lfx_has(&f, "ek_val_int((int64_t)ek_set_len("));
    EK_CHECK(lfx_has(&f, "ek_set_to_list("));
    /* no identity stamping rides the set rows */
    EK_CHECK(lfx_lacks(&f, "ekT_std$Json$Null"));
    lfx_end(&f);
}

static void vector_rows_route(void)
{
    lfx f;

    lfx_begin(&f,
              "main: () -> _ =\n"
              "    v: Vector Int = Vector.of({3, 1})\n"
              "    w := v.push(9)\n"
              "    g := w.get(0)\n"
              "    s := w.set(1, 5)\n"
              "    n := v.length()\n"
              "    xs := v.to_list()\n"
              "    _ := g\n"
              "    _ := s\n"
              "    _ := n\n"
              "    _ := xs\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ek_vec_of("));
    EK_CHECK(lfx_has(&f, "ek_vec_push("));
    /* get wraps the status contract into Some and None */
    EK_CHECK(lfx_has(&f, "ek_vec_get("));
    /* set bridges the fresh object or NULL into Some and None */
    EK_CHECK(lfx_has(&f, "= ek_vec_set("));
    /* the count routes to the runtime and widens into the Int slot */
    EK_CHECK(lfx_has(&f, "ek_vec_len("));
    EK_CHECK(lfx_has(&f, "ek_vec_to_list("));
    lfx_end(&f);
}

static void radix_rows_route(void)
{
    lfx f;

    lfx_begin(&f,
              "main: () -> _ =\n"
              "    n: Int = 255\n"
              "    u: UInt = 7\n"
              "    a = n.to_hex()\n"
              "    b = n.to_oct()\n"
              "    c = n.to_bin()\n"
              "    d = u.to_hex()\n"
              "    _ := a\n"
              "    _ := b\n"
              "    _ := c\n"
              "    _ := d\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the radix channel renders fresh objects over the value slots */
    EK_CHECK(lfx_has(&f, "ek_val_ref(ek_show_radix("));
    EK_CHECK(lfx_has(&f, ", 16));"));
    EK_CHECK(lfx_has(&f, ", 8));"));
    EK_CHECK(lfx_has(&f, ", 2));"));
    lfx_end(&f);
}

static void hex_pad_rows_route(void)
{
    lfx f;

    lfx_begin(&f,
              "main: () -> _ =\n"
              "    w: I32 = 255\n"
              "    x: U64 = 1\n"
              "    a = w.to_hex_pad(4)\n"
              "    b = x.to_hex_pad(2)\n"
              "    _ := a\n"
              "    _ := b\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the padded channel takes the value and the width value */
    EK_CHECK(lfx_has(&f, "ek_val_ref(ek_show_hex_pad("));
    lfx_end(&f);
}

static void dict_traversal_rows_route(void)
{
    lfx f;

    lfx_begin(&f,
              "main: () -> _ =\n"
              "    d: [Int]String = {2: \"b\", 1: \"a\"}\n"
              "    s: String = d.fold(\"\", (acc, k, v): acc ++ v)\n"
              "    e = d.each((k, v): _)\n"
              "    m: [Int]String = d.map((k, v): v ++ \"!\")\n"
              "    q: [Int]String = d.filter((k, v): k > 1)\n"
              "    _ := s\n"
              "    _ := e\n"
              "    _ := m\n"
              "    _ := q\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the traversals ride the closure ABI like the range combinators */
    EK_CHECK(lfx_has(&f, "ek_dict_fold("));
    EK_CHECK(lfx_has(&f, "ek_dict_each("));
    EK_CHECK(lfx_has(&f, "ek_dict_map("));
    EK_CHECK(lfx_has(&f, "ek_dict_filter("));
    EK_CHECK(lfx_has(&f, "ek_closure_new(ekl_"));
    lfx_end(&f);
}

static void json_parse_stamps_identities(void)
{
    lfx f;

    lfx_begin(&f,
              "main: () -> _ =\n"
              "    r = case Json.parse(\"[1, true]\"):\n"
              "        Ok _ -> 1\n"
              "        Err _ -> 0 - 1\n"
              "    _ := r\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the parse row routes to the runtime reader */
    EK_CHECK(lfx_has(&f, "ek_json_parse("));
    /* the routing stamps the Json and ParseError families: the typeinfo
       identities join the unit with their drops and no helpers, the
       shape the runtime's builders share */
    EK_CHECK(lfx_has(&f, "const ek_typeinfo ekT_std$Json$Null ="));
    EK_CHECK(lfx_has(&f, "const ek_typeinfo ekT_std$Json$Obj ="));
    EK_CHECK(lfx_has(&f, "const ek_typeinfo ekT_std$ParseError$Empty ="));
    EK_CHECK(lfx_has(&f, "const ek_typeinfo ekT_std$ParseError$"
                         "OutOfRange ="));
    EK_CHECK(lfx_lacks(&f, "ekn_std$Json$Null"));
    lfx_end(&f);
}

static void json_stamp_fires_only_on_routing(void)
{
    lfx f;

    /* a program without the parse row defines none of the identities */
    lfx_begin(&f,
              "main: (out: Out) ! =\n"
              "    r = case Int.parse(\"42\"):\n"
              "        Ok n -> n\n"
              "        Err _ -> 0 - 1\n"
              "    out.println(show(r))\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ek_parse_int("));
    EK_CHECK(lfx_lacks(&f, "ekT_std$Json$"));
    EK_CHECK(lfx_lacks(&f, "ek_json_parse"));
    lfx_end(&f);
}

int main(void)
{
    set_rows_route();
    vector_rows_route();
    radix_rows_route();
    hex_pad_rows_route();
    dict_traversal_rows_route();
    json_parse_stamps_identities();
    json_stamp_fires_only_on_routing();
    return ek_test_report("lower/rows");
}
