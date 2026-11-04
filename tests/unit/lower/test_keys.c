#include "common.h"

/* The structural keying: a family a dict or set keys on stamps the
   eq and hash of its applied type into the typeinfo text, and the
   generated hash mirrors the equality walk -- the family identity
   first, then each field's tag and payload. A family no key domain
   touches keeps the two NULL fields. */

static const char *record_src =
    "P: type = [x: Int, y: Int]\n"
    "main: () -> _ =\n"
    "    p: P = {x: 1, y: 2}\n"
    "    d: [P]Int = {}\n"
    "    d := d.insert(p, 5)\n"
    "    _ := d\n"
    "    _\n";

static void record_stamps_eq_and_hash(void)
{
    lfx f;

    lfx_begin(&f, record_src);
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the signatures stand ahead of the typeinfo that names them */
    EK_CHECK(lfx_has(&f, "static int ekq_N$test$P(ek_val a, ek_val b);\n"
                         "static unsigned long ekh_N$test$P(ek_val v);\n"
                         "const ek_typeinfo ekT_test$P = "
                         "{ EK_KIND_STRUCT, drop_test$P, "
                         "ekq_N$test$P, ekh_N$test$P };"));
    /* the hash mixes the family identity, then each field's tag and
       payload */
    EK_CHECK(lfx_has(&f, "static unsigned long ekh_N$test$P(ek_val v)\n"
                         "{\n"
                         "    unsigned long h = 14695981039346656037ULL;"
                         "\n\n"
                         "    h = ek_hash_mix(h, (unsigned long)"
                         "v.as.ref->info);\n"));
    EK_CHECK(lfx_has(&f, "h = ek_hash_mix(h, (unsigned long)"
                         "((ekS_test$P *)v.as.ref)->f_x.tag);\n"
                         "    h = ek_hash_mix(h, (unsigned long)"
                         "((ekS_test$P *)v.as.ref)->f_x.as.i);"));
    EK_CHECK(lfx_has(&f, "h = ek_hash_mix(h, (unsigned long)"
                         "((ekS_test$P *)v.as.ref)->f_y.as.i);"));
    lfx_end(&f);
}

static const char *untouched_src =
    "Person: type = [name: String, age: Int]\n"
    "main: () -> _ =\n"
    "    p: Person = {age: 36, name: \"ada\"}\n"
    "    n = p.age\n"
    "    s = n + 1\n"
    "    if s == 37:\n"
    "        _\n"
    "    _\n";

static void families_no_key_touches_stay_null(void)
{
    lfx f;

    lfx_begin(&f, untouched_src);
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "const ek_typeinfo ekT_test$Person = "
                         "{ EK_KIND_STRUCT, drop_test$Person, NULL, NULL };"));
    EK_CHECK(lfx_lacks(&f, "ekh_"));
    lfx_end(&f);
}

static const char *adt_src =
    "V: type = S String | M Float\n"
    "main: () -> _ =\n"
    "    d: [V]Int = {}\n"
    "    d := d.insert(S \"x\", 1)\n"
    "    d := d.insert(M 2.0, 2)\n"
    "    _ := d\n"
    "    _\n";

static void adt_stamps_every_constructor(void)
{
    lfx f;

    lfx_begin(&f, adt_src);
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* each constructor's typeinfo carries the applied type's services */
    EK_CHECK(lfx_has(&f, "const ek_typeinfo ekT_test$V$S = "
                         "{ EK_KIND_STRUCT, drop_test$V$S, "
                         "ekq_N$test$V, ekh_N$test$V };"));
    EK_CHECK(lfx_has(&f, "const ek_typeinfo ekT_test$V$M = "
                         "{ EK_KIND_STRUCT, drop_test$V$M, "
                         "ekq_N$test$V, ekh_N$test$V };"));
    /* the hash dispatches on the constructor identity and mixes the
       payload in its own form: a String through the child hook, a
       Float by the canonicalized bits */
    EK_CHECK(lfx_has(&f, "if (v.as.ref->info == &ekT_test$V$S) {\n"
                         "    h = ek_hash_mix(h, (unsigned long)"
                         "((ekS_test$V$S *)v.as.ref)->p0.tag);\n"));
    EK_CHECK(lfx_has(&f, "h = ek_hash_mix(h, "
                         "((ekS_test$V$S *)v.as.ref)->p0.as.ref->info->hash"
                         "(((ekS_test$V$S *)v.as.ref)->p0));"));
    EK_CHECK(lfx_has(&f, "if (x == 0.0) {\n            x = 0.0;\n"
                         "        }\n"
                         "        memcpy(&b, &x, sizeof b);"));
    lfx_end(&f);
}

static const char *nested_src =
    "Inner: type = [a: Int]\n"
    "Outer: type = [i: Inner, s: String]\n"
    "main: () -> _ =\n"
    "    o: Outer = {i: {a: 1}, s: \"x\"}\n"
    "    d: [Outer]Int = {}\n"
    "    d := d.insert(o, 1)\n"
    "    _ := d\n"
    "    _\n";

static void nested_refs_hash_through_the_child_hook(void)
{
    lfx f;

    lfx_begin(&f, nested_src);
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "const ek_typeinfo ekT_test$Outer = "
                         "{ EK_KIND_STRUCT, drop_test$Outer, "
                         "ekq_N$test$Outer, ekh_N$test$Outer };"));
    /* a reference field hashes through the child typeinfo's hook */
    EK_CHECK(lfx_has(&f, "if (((ekS_test$Outer *)v.as.ref)->f_i.tag == "
                         "EK_VAL_REF"));
    EK_CHECK(lfx_has(&f, "((ekS_test$Outer *)v.as.ref)->f_i.as.ref->"
                         "info->hash(((ekS_test$Outer *)v.as.ref)->f_i)"));
    /* the identity fallback keeps the address */
    EK_CHECK(lfx_has(&f, "h = ek_hash_mix(h, (unsigned long)"
                         "((ekS_test$Outer *)v.as.ref)->f_i.as.ref);"));
    lfx_end(&f);
}

static const char *anon_src =
    "main: () -> _ =\n"
    "    e: [[x: Int]]Int = {}\n"
    "    e := e.insert({x: 2}, 7)\n"
    "    _ := e\n"
    "    _\n";

static void anon_record_stamps_too(void)
{
    lfx f;

    lfx_begin(&f, anon_src);
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "const ek_typeinfo ekT_rec0 = "
                         "{ EK_KIND_STRUCT, drop_rec0, "
                         "ekq_R1$x$Int, ekh_R1$x$Int };"));
    lfx_end(&f);
}

static const char *set_src =
    "V: type = N | S String\n"
    "main: () -> _ =\n"
    "    s = Set.of({S \"a\", S \"a\", N})\n"
    "    _ := s\n"
    "    _\n";

static void set_elements_stamp_too(void)
{
    lfx f;

    lfx_begin(&f, set_src);
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the element domain stamps through Set.of the way a key domain
       stamps through the dictionary; the field-less variant carries
       the NULL drop beside its hooks */
    EK_CHECK(lfx_has(&f, "const ek_typeinfo ekT_test$V$N = "
                         "{ EK_KIND_STRUCT, NULL, "
                         "ekq_N$test$V, ekh_N$test$V };"));
    EK_CHECK(lfx_has(&f, "const ek_typeinfo ekT_test$V$S = "
                         "{ EK_KIND_STRUCT, drop_test$V$S, "
                         "ekq_N$test$V, ekh_N$test$V };"));
    EK_CHECK(lfx_has(&f, "ek_set_of("));
    lfx_end(&f);
}

static const char *unit_key_src =
    "main: () -> _ =\n"
    "    d: [[]Int]Int = {}\n"
    "    _ := d\n"
    "    _\n";

static void undetermined_domains_keep_the_identity_reading(void)
{
    lfx f;

    lfx_begin(&f, unit_key_src);
    EK_CHECK(lfx_lower_clean(&f));
    /* a list key domain has no per-value family: no hooks stamp, and
       the runtime's identity fallback stands */
    EK_CHECK(lfx_lacks(&f, "ekh_"));
    lfx_end(&f);
}

int main(void)
{
    record_stamps_eq_and_hash();
    families_no_key_touches_stay_null();
    adt_stamps_every_constructor();
    nested_refs_hash_through_the_child_hook();
    anon_record_stamps_too();
    set_elements_stamp_too();
    undetermined_domains_keep_the_identity_reading();
    return ek_test_report("lower/keys");
}
