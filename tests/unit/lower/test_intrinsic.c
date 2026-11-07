#include "common.h"

/* The intrinsic dispatch: the routed runtime symbols, the Option and
   Result wrapping through the std constructors, and the rows that
   keep the unsupported path. */

static void uint_to_int_wraps(void)
{
    lfx f;

    lfx_begin(&f,
              "main: () -> _ =\n"
              "    u: UInt = 7\n"
              "    r = case u.to_int():\n"
              "        Some n -> n\n"
              "        None -> 0 - 1\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the narrowing row calls the runtime and wraps the hit and the
       miss through the std constructors */
    EK_CHECK(lfx_has(&f, "ek_try_to_int("));
    EK_CHECK(lfx_has(&f, "ekn_std$Option$Some(ek_val_int("));
    EK_CHECK(lfx_has(&f, "ekn_std$Option$None();"));
    /* the constructor family joined the unit */
    EK_CHECK(lfx_has(&f, "ekS_std$Option$Some"));
    lfx_end(&f);
}

static void parse_wraps_result(void)
{
    lfx f;

    lfx_begin(&f,
              "main: () -> _ =\n"
              "    r = case Int.parse(\"42\"):\n"
              "        Ok n -> n\n"
              "        Err _ -> 0 - 1\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ek_parse_int("));
    EK_CHECK(lfx_has(&f, "ekn_std$Result$Ok(ek_val_int("));
    /* the parse kinds map onto the ParseError variants; each miss arm
       binds the error to its own named temp and releases it, and the
       empty detail is the interned empty literal, kept and released
       beside the Invalid it lands in */
    EK_CHECK(lfx_has(&f, "        ek_val t4 = ekn_std$ParseError$Empty();\n"
                         "        t1 = ekn_std$Result$Err(t4);\n"
                         "        ek_release(t4);"));
    EK_CHECK(lfx_has(&f, "        ek_val t7 = ekn_std$ParseError$"
                         "OutOfRange();\n"
                         "        t1 = ekn_std$Result$Err(t7);\n"
                         "        ek_release(t7);"));
    EK_CHECK(lfx_has(&f, "        ek_val t5 = ek_keep(ekc"));
    EK_CHECK(lfx_has(&f, "        ek_val t6 = ekn_std$ParseError$Invalid("
                         "t5);\n"
                         "        t1 = ekn_std$Result$Err(t6);\n"
                         "        ek_release(t6);\n"
                         "        ek_release(t5);"));
    lfx_end(&f);
}

static void radix_parses(void)
{
    lfx f;

    lfx_begin(&f,
              "main: () -> _ =\n"
              "    r = case Int.parse_hex(\"2a\"):\n"
              "        Ok n -> n\n"
              "        Err _ -> 0 - 1\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ek_parse_int_radix("));
    EK_CHECK(lfx_has(&f, ", 16, &"));
    lfx_end(&f);
}

static void length_rows_count(void)
{
    lfx f;

    lfx_begin(&f,
              "main: () -> _ =\n"
              "    n = \"héllo\".length()\n"
              "    d: [Int]Int = {1: 2}\n"
              "    m = d.length()\n"
              "    _ := n\n"
              "    _ := m\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ek_val_int((int64_t)ek_str_length("));
    EK_CHECK(lfx_has(&f, "ek_val_int((int64_t)ek_dict_len("));
    lfx_end(&f);
}

static void dict_get_wraps_borrowed(void)
{
    lfx f;

    lfx_begin(&f,
              "main: () -> _ =\n"
              "    d: [Int]Int = {7: 1}\n"
              "    r = case d.get(7):\n"
              "        Some v -> v\n"
              "        None -> 0 - 1\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the borrowed hit is retained into the payload and the keep dies
       once the constructor's own retention stands */
    EK_CHECK(lfx_has(&f, "if (ek_dict_get("));
    EK_CHECK(lfx_has(&f, "ek_keep(t7)"));
    EK_CHECK(lfx_has(&f, "ekn_std$Option$Some(t8);"));
    EK_CHECK(lfx_has(&f, "ek_release(t8);"));
    lfx_end(&f);
}

static void string_and_byte_objects(void)
{
    lfx f;

    lfx_begin(&f,
              "main: () -> _ =\n"
              "    h = \"AB\".to_bytes().to_hex()\n"
              "    _ := h\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* both object results are born through the reference constructor */
    EK_CHECK(lfx_has(&f, "ek_val_ref(ek_str_to_bytes("));
    EK_CHECK(lfx_has(&f, "ek_val_ref(ek_bytes_to_hex("));
    lfx_end(&f);
}

static void decode_wraps_detail(void)
{
    lfx f;

    lfx_begin(&f,
              "main: () -> _ =\n"
              "    r = case Bytes.from_hex(\"zz\"):\n"
              "        Ok b -> b.to_hex()\n"
              "        Err _ -> \"no\"\n"
              "    _ := r\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the detail object becomes the Invalid reason and dies with its
       own reference: the constructor's argument temp is the one the
       wrap releases */
    EK_CHECK(lfx_has(&f, "ek_bytes_from_hex("));
    {
        const char *def = strstr(f.low.text,
                                 "ekn_std$ParseError$Invalid(ek_val p0)");
        const char *body = def ? def + strlen("ekn_std$ParseError$Invalid"
                                              "(ek_val p0)") : NULL;
        const char *call = body ? strstr(body, "ekn_std$ParseError$Invalid(")
                                : NULL;
        char temp[16];
        char rel[48];

        EK_CHECK(def != NULL);
        if (def != NULL) {
            EK_CHECK(call != NULL);
        }
        if (call != NULL) {
            const char *arg = call + strlen("ekn_std$ParseError$Invalid(");
            size_t n = strcspn(arg, ")");

            EK_CHECK(n > 0 && n < sizeof temp);
            if (n > 0 && n < sizeof temp) {
                memcpy(temp, arg, n);
                temp[n] = 0;
                snprintf(rel, sizeof rel, "ek_release(%s);", temp);
                EK_CHECK(strstr(f.low.text, rel) != NULL);
            }
        }
    }
    lfx_end(&f);
}

static void widening_identity(void)
{
    lfx f;

    lfx_begin(&f,
              "main: () -> _ =\n"
              "    x: I8 = 5\n"
              "    r = Int.from_i8(x)\n"
              "    _ := r\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the widening row keeps the argument: the word already travels
       in the slot of its domain */
    EK_CHECK(lfx_has(&f, "= ek_keep(t"));
    lfx_end(&f);
}

static void set_algebra_routes(void)
{
    lfx f;

    /* the set algebra routes to the runtime entries over the receiver
       and the operand */
    lfx_begin(&f,
              "main: () -> _ =\n"
              "    s = Set.of({1})\n"
              "    u = s.union(s)\n"
              "    i = s.intersect(s)\n"
              "    d = s.diff(s)\n"
              "    _ := u\n"
              "    _ := i\n"
              "    _ := d\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_errors(&f) == 0);
    EK_CHECK(lfx_has(&f, "ek_set_union("));
    EK_CHECK(lfx_has(&f, "ek_set_intersect("));
    EK_CHECK(lfx_has(&f, "ek_set_diff("));
    lfx_end(&f);
}

static void std_source_uses_the_rows(void)
{
    lfx f;

    /* String.at is std source over the scalar rows: its body lowers
       through the same dispatch */
    lfx_begin(&f,
              "main: () -> _ =\n"
              "    r = case \"héllo\".at(1):\n"
              "        Some c -> c.to_code()\n"
              "        None -> 0 - 1\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the std member is the callee; the rows feed it from inside */
    EK_CHECK(lfx_has(&f, "ekf_std$String$at("));
    EK_CHECK(lfx_has(&f, "ek_str_chars("));
    EK_CHECK(lfx_has(&f, "ek_char_to_code("));
    lfx_end(&f);
}

/* A wrong-arity call still reaches the dispatch: check refuses the
   arity, the pipeline lowers anyway, and the routed row answers with
   its own malformed-call refusal while the arguments die. */
static void wrong_arity_refuses(void)
{
    static const char *want =
        "the call of `String.length` is malformed";
    lfx f;
    size_t i;
    int arity = 0;
    int refusal = 0;

    lfx_begin(&f,
              "main: () -> _ =\n"
              "    n = \"héllo\".length(1)\n"
              "    _ := n\n"
              "    _\n");
    EK_CHECK(!lfx_check_clean(&f));
    for (i = 0; i < f.cdn; i++) {
        if (f.cds[i].code && strcmp(f.cds[i].code, "E-call-arity") == 0) {
            arity = 1;
        }
    }
    EK_CHECK(arity);
    EK_CHECK(!lfx_lower_clean(&f));
    for (i = 0; i < f.ldn; i++) {
        if (f.lds[i].message && strcmp(f.lds[i].message, want) == 0) {
            refusal = 1;
        }
    }
    EK_CHECK(refusal);
    /* the drained run still yields the unit: the receiver and the
       argument evaluate and die */
    EK_CHECK(f.low.text != NULL);
    lfx_end(&f);
}

int main(void)
{
    uint_to_int_wraps();
    parse_wraps_result();
    radix_parses();
    length_rows_count();
    dict_get_wraps_borrowed();
    string_and_byte_objects();
    decode_wraps_detail();
    widening_identity();
    set_algebra_routes();
    std_source_uses_the_rows();
    wrong_arity_refuses();
    return ek_test_report("lower/intrinsic");
}
