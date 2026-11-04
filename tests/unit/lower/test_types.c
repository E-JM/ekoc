#include "common.h"

/* The generated struct layer: the emitted shapes of declared types and
   constructor families, the construction helpers' call text, the field
   access shape, and the ownership of a literal that transfers into a
   binding. */

static const char *record_src =
    "Person: type = [name: String, age: Int]\n"
    "main: () -> _ =\n"
    "    p: Person = {age: 36, name: \"ada\"}\n"
    "    n = p.age\n"
    "    s = n + 1\n"
    "    if s == 37:\n"
    "        _\n"
    "    _\n";

static void record_shape(void)
{
    lfx f;

    lfx_begin(&f, record_src);
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "typedef struct { ek_obj obj; "
                           "ek_val f_name; ek_val f_age; } "
                           "ekS_test$Person;"));
    /* the drop releases the fields in declaration order */
    EK_CHECK(lfx_has(&f, "ek_release(((ekS_test$Person *)o)->f_name);\n"
                         "    ek_release(((ekS_test$Person *)o)->f_age);"));
    EK_CHECK(lfx_has(&f, "const ek_typeinfo ekT_test$Person = "
                         "{ EK_KIND_STRUCT, drop_test$Person, NULL, NULL };"));
    EK_CHECK(lfx_has(&f, "static ek_val ekn_test$Person$rec(ek_val p0, "
                         "ek_val p1)"));
    EK_CHECK(lfx_has(&f, "o->f_name = ek_keep(p0);"));
    EK_CHECK(lfx_has(&f, "o->f_age = ek_keep(p1);"));
    /* the items map by name, not by written order: the fields pass in
       declaration order while the temps were minted in source order */
    EK_CHECK(lfx_has(&f, "ek_val t2 = ekn_test$Person$rec(t1, t0);"));
    /* the literal transfers into the binding; the access is an owned
       copy out of the struct, read through the borrowed local */
    EK_CHECK(lfx_has(&f, "ek_val p = t2;"));
    EK_CHECK(lfx_has(&f, "ek_val t3 = ek_keep(((ekS_test$Person *)"
                         "p.as.ref)->f_age);"));
    lfx_end(&f);
}

static const char *adt_src =
    "sink: (o: Option Int) -> Int = 0\n"
    "main: () -> _ =\n"
    "    a = sink(Some 36)\n"
    "    b = sink(None)\n"
    "    if a == b:\n"
    "        _\n"
    "    _\n";

static void adt_shape(void)
{
    lfx f;

    lfx_begin(&f, adt_src);
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* each constructor carries its own struct, drop, and typeinfo; a
       payload constructor stores p0, a nullary one is the bare header
       with the NULL drop of a family that holds no child references */
    EK_CHECK(lfx_has(&f, "typedef struct { ek_obj obj; ek_val p0; } "
                         "ekS_std$Option$Some;"));
    EK_CHECK(lfx_has(&f, "typedef struct { ek_obj obj; } "
                         "ekS_std$Option$None;"));
    EK_CHECK(lfx_has(&f, "ek_release(((ekS_std$Option$Some *)o)->p0);"));
    EK_CHECK(lfx_has(&f, "const ek_typeinfo ekT_std$Option$None = "
                         "{ EK_KIND_STRUCT, NULL, NULL, NULL };"));
    EK_CHECK(lfx_lacks(&f, "drop_std$Option$None"));
    EK_CHECK(lfx_has(&f, "const ek_typeinfo ekT_std$Option$Some = "
                         "{ EK_KIND_STRUCT, drop_std$Option$Some, "
                         "NULL, NULL };"));
    EK_CHECK(lfx_has(&f, "static ek_val ekn_std$Option$Some(ek_val p0)"));
    EK_CHECK(lfx_has(&f, "o->obj.info = &ekT_std$Option$Some;"));
    EK_CHECK(lfx_has(&f, "o->obj.rc = 1;"));
    EK_CHECK(lfx_has(&f, "o->p0 = ek_keep(p0);"));
    EK_CHECK(lfx_has(&f, "return ek_val_ref(&o->obj);"));
    EK_CHECK(lfx_has(&f, "static ek_val ekn_std$Option$None(void)"));
    /* the call site releases the payload temp once the helper's own
       retention stands, and the std surface stays untouched */
    EK_CHECK(lfx_has(&f, "ek_val t1 = ekn_std$Option$Some(t0);\n"
                         "    ek_release(t0);"));
    EK_CHECK(lfx_has(&f, "ekn_std$Option$None();"));
    EK_CHECK(lfx_lacks(&f, "ekf_std$"));
    lfx_end(&f);
}

static const char *tuple_src =
    "Pair: type = [Int, Int]\n"
    "take: (t: [Int, Int]) -> Int = 0\n"
    "takep: (t: Pair) -> Int = 0\n"
    "main: () -> _ =\n"
    "    a = take({1, 2})\n"
    "    v: Pair = {5, 6}\n"
    "    b = takep(v)\n"
    "    u: [Int, Int] = {3, 4}\n"
    "    c = take(u)\n"
    "    r = a + b + c\n"
    "    if r == 0:\n"
    "        _\n"
    "    _\n";

static void tuple_shape(void)
{
    lfx f;

    lfx_begin(&f, tuple_src);
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* a declared tuple-shaped type lays its components out positionally
       in the declaration order; the anonymous shape of the same arity
       shares the layout under its own name */
    EK_CHECK(lfx_has(&f, "typedef struct { ek_obj obj; ek_val f_0; "
                         "ek_val f_1; } ekS_test$Pair;"));
    EK_CHECK(lfx_has(&f, "typedef struct { ek_obj obj; ek_val f_0; "
                         "ek_val f_1; } ekS_tup_2;"));
    EK_CHECK(lfx_has(&f, "static ek_val ekn_test$Pair$rec(ek_val p0, "
                         "ek_val p1)"));
    EK_CHECK(lfx_has(&f, "static ek_val ekn_tup_2(ek_val p0, ek_val p1)"));
    EK_CHECK(lfx_has(&f, "o->f_0 = ek_keep(p0);"));
    EK_CHECK(lfx_has(&f, "ekn_test$Pair$rec(t"));
    EK_CHECK(lfx_has(&f, "ekn_tup_2(t"));
    lfx_end(&f);
}

static const char *list_dict_src =
    "sum: (xs: []Int) -> Int = 0\n"
    "look: (d: [Int]Int) -> Int = 0\n"
    "main: () -> _ =\n"
    "    a = sum({1, 2, 3})\n"
    "    b = sum({})\n"
    "    c = look({1: 10, 2: 20})\n"
    "    e = look({})\n"
    "    r = a + b + c + e\n"
    "    if r == 0:\n"
    "        _\n"
    "    _\n";

static void list_dict_shape(void)
{
    lfx f;

    lfx_begin(&f, list_dict_src);
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the elements evaluate to temps in source order, then one fold
       builds the spine from the nil end */
    EK_CHECK(lfx_has(&f, "static ek_val ekn_list(ek_val *elems, "
                         "size_t n)"));
    EK_CHECK(lfx_has(&f, "ek_val t3[] = {t0, t1, t2};"));
    EK_CHECK(lfx_has(&f, "ek_val t4 = ekn_list(t3, 3);"));
    EK_CHECK(lfx_has(&f, "ek_release(t2);\n    ek_release(t1);\n"
                         "    ek_release(t0);"));
    /* the empty literal is the nil value */
    EK_CHECK(lfx_has(&f, "ek_val t6 = ek_nil_val();"));
    /* the dictionary entries' keys and values evaluate in source
       order, the set chain builds through them, and each intermediate
       releases once its replacement exists */
    EK_CHECK(lfx_has(&f, "ek_val t12 = ek_dict_new();"));
    EK_CHECK(lfx_has(&f, "ek_val t13 = ek_dict_set(t12, t8, t9);\n"
                         "    ek_release(t12);"));
    EK_CHECK(lfx_has(&f, "ek_val t14 = ek_dict_set(t13, t10, t11);"));
    EK_CHECK(lfx_has(&f, "ek_release(t9);\n    ek_release(t8);"));
    /* the empty literal is the empty dictionary */
    EK_CHECK(lfx_has(&f, "ek_val t16 = ek_dict_new();"));
    lfx_end(&f);
}

static void nested_list_shape(void)
{
    lfx f;

    lfx_begin(&f, "sum: (xs: [][]Int) -> Int = 0\n"
                  "main: () -> _ =\n"
                  "    n = sum({{1}, {2, 3}})\n"
                  "    if n == 0:\n"
                  "        _\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the inner literals fold first and their temps feed the outer
       array */
    EK_CHECK(lfx_has(&f, "ekn_list("));
    EK_CHECK_U64(f.low.fn_count, 2);
    lfx_end(&f);
}

/* The one-directional shape adaptation: a nominal record flowing into
   an anonymous record expectation rebuilds through the destination
   family, reading every field off the source family's slots, at a
   binding annotation and at a call argument alike. */
static void shape_adaptation_rebuilds(void)
{
    lfx f;

    lfx_begin(&f,
              "Human: type = [name: String, age: Int]\n"
              "_age_of: (p: [name: String, age: Int]) -> Int = p.age\n"
              "main: () -> _ =\n"
              "    h: Human = {name: \"ada\", age: 36}\n"
              "    view: [name: String, age: Int] = h\n"
              "    _ := _age_of(h)\n"
              "    _ := view.age\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the anonymous family births from owned reads of the nominal's
       slots, both at the binding and at the call */
    EK_CHECK(lfx_has(&f, "ekn_rec0("));
    EK_CHECK(lfx_has(&f, "ek_keep(((ekS_test$Human *)"));
    /* the nominal object never aliases into the anonymous binding */
    EK_CHECK(lfx_lacks(&f, "ek_val view = ek_keep(h);"));
    lfx_end(&f);
}

/* A receiver-form head the resolver bridged reads as the constructor
   it names once check anchors the nominal: the Json Null head lowers
   to its nullary birth. */
static void bridged_ctor_head_reads(void)
{
    lfx f;

    lfx_begin(&f,
              "main: () -> _ =\n"
              "    s = Null.show()\n"
              "    _ := s\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ekn_std$Json$Null()"));
    lfx_end(&f);
}

int main(void)
{
    record_shape();
    adt_shape();
    tuple_shape();
    list_dict_shape();
    nested_list_shape();
    shape_adaptation_rebuilds();
    bridged_ctor_head_reads();
    return ek_test_report("lower/types");
}
