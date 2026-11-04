#include "common.h"

/* The derived services in the emitted text: one service per family and
   applied type, the applied arguments spelled into the name, forward
   declarations ahead of the bodies so a recursive type's service may
   call itself, the comparison operators reading the same services over
   composite rows, and with rebuilding the record through its
   construction helper. */

/* A generic instantiation is its own service: the applied argument
   joins the spelling, so Option Int and Option String never share. */
static void specialization_naming(void)
{
    lfx f;

    lfx_begin(&f,
              "by_int: (a: Option Int, b: Option Int) -> Int =\n"
              "    case a.eq(b):\n"
              "        True -> 1\n"
              "        False -> 0\n"
              "by_str: (a: Option String, b: Option String) -> Int =\n"
              "    case a.eq(b):\n"
              "        True -> 1\n"
              "        False -> 0\n"
              "main: () -> _ =\n"
              "    _ := by_int(None, None)\n"
              "    _ := by_str(None, None)\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "static int ekq_N$std$Option$Int(ek_val a, "
                         "ek_val b);\n"));
    EK_CHECK(lfx_has(&f, "static int ekq_N$std$Option$String(ek_val a, "
                         "ek_val b);\n"));
    EK_CHECK(lfx_has(&f, "ekq_N$std$Option$Int("));
    EK_CHECK(lfx_has(&f, "ekq_N$std$Option$String("));
    lfx_end(&f);
}

/* A recursive type's show declares itself ahead of its body, and the
   body calls its own name over the spine's tail. */
static void recursive_forward_decl(void)
{
    lfx f;

    lfx_begin(&f,
              "Chain: type = End | Link Chain\n"
              "main: (o: Out) ! =\n"
              "    c: Chain = Link End\n"
              "    o.println(c.show())\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "static ek_obj *ekd_show_N$test$Chain(ek_val v);\n"));
    EK_CHECK(lfx_has(&f, "static ek_obj *ekd_inspect_N$test$Chain(ek_val v);"
                       "\n"));
    EK_CHECK(lfx_has(&f, "ekd_inspect_N$test$Chain(((ekS_test$Chain$Link *)"
                       "v.as.ref)->p0)"));
    /* the dispatch tests the constructor families' typeinfo */
    EK_CHECK(lfx_has(&f, "v.as.ref->info == &ekT_test$Chain$End"));
    EK_CHECK(lfx_has(&f, "v.as.ref->info == &ekT_test$Chain$Link"));
    lfx_end(&f);
}

/* Composite equality arrives as the derived member and reads the eq
   service; the operators exist on the value layer alone, where the
   object slots (String, Bytes) read the derived services and the
   scalar rows keep the runtime comparisons. */
static void operator_routing(void)
{
    lfx f;

    lfx_begin(&f,
              "Tree: type = Leaf Int | Fork Tree Tree\n"
              "main: (o: Out) ! =\n"
              "    a: Tree = Leaf 1\n"
              "    b: Tree = Leaf 2\n"
              "    t1: Tree = Fork a b\n"
              "    t2: Tree = Fork a b\n"
              "    o.println(t1.eq(t2).show())\n"
              "    o.println(t1.ne(t2).show())\n"
              "    o.println((\"abc\" < \"abd\").show())\n"
              "    o.println((\"ab\" == \"ab\").show())\n"
              "    o.println((1 < 2).show())\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ek_bool(ekq_N$test$Tree("));
    EK_CHECK(lfx_has(&f, "ek_bool(!ekq_N$test$Tree("));
    EK_CHECK(lfx_has(&f, "ek_bool(eko_String("));
    EK_CHECK(lfx_has(&f, "ek_bool(ekq_String("));
    EK_CHECK(lfx_has(&f, "ek_num_lt("));
    lfx_end(&f);
}

/* with constructs through the record family's helper: the literal's
   named fields in its argument positions, the unnamed fields read off
   the receiver temp. */
static void with_shape(void)
{
    lfx f;

    lfx_begin(&f,
              "Money: type = [cents: Int, cur: String]\n"
              "main: () -> _ =\n"
              "    m: Money = {cents: 12500, cur: \"USD\"}\n"
              "    n = m.with({cents: 99})\n"
              "    _ := n.cents\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ekn_test$Money$rec(t"));
    /* the field reads borrow the frame's own record: the receiver
       keeps nothing, the slot keeps its own copy */
    EK_CHECK(lfx_has(&f, "ek_keep(((ekS_test$Money *)(t3).as.ref)->f_cur)"));
    EK_CHECK(lfx_has(&f, "ek_keep(((ekS_test$Money *)n.as.ref)->f_cents)"));
    lfx_end(&f);
}

/* The ordering member of an algebraic type reads the declaration-order
   service; the display pair answers the scalar rows through the
   runtime family. */
static void ordering_and_display_members(void)
{
    lfx f;

    lfx_begin(&f,
              "Sex: type = Man | Woman\n"
              "main: (o: Out) ! =\n"
              "    a: Sex = Man\n"
              "    o.println(a.lt(Woman).show())\n"
              "    o.println(show(3))\n"
              "    o.println(\"wd\".inspect())\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ek_bool(eko_N$test$Sex("));
    EK_CHECK(lfx_has(&f, "ekd_show_Int(ek_val v)"));
    EK_CHECK(lfx_has(&f, "return ek_show_int(v);"));
    EK_CHECK(lfx_has(&f, "ek_str_quote(v.as.ref, '\"')"));
    lfx_end(&f);
}

/* A tuple payload walks positionally: the eq over a variant's tuple
   reads the slots the typedef numbers, never a written field name. */
static void tuple_payload_equality(void)
{
    lfx f;

    lfx_begin(&f,
              "Tree: type t = Leaf t | Node [Tree t, Tree t]\n"
              "main: (o: Out) ! =\n"
              "    a: Tree Int = Node {Leaf 1, Leaf 2}\n"
              "    b: Tree Int = Node {Leaf 1, Leaf 2}\n"
              "    o.println(a.eq(b).show())\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "((ekS_tup_2 *)a.as.ref)->f_0"));
    EK_CHECK(lfx_has(&f, "((ekS_tup_2 *)b.as.ref)->f_1"));
    EK_CHECK(lfx_lacks(&f, "f_(null)"));
    lfx_end(&f);
}

/* The capability channel answers the two display channels with the
   <cap> form: the show and inspect requests mint their services, the
   show body forwards to the inspect body, and the value reads through
   no structure. */
static void capability_display(void)
{
    lfx f;

    lfx_begin(&f,
              "main: (o: Out) ! =\n"
              "    o.println(Out.null().show())\n"
              "    o.println(show(Fs.null()))\n"
              "    o.println(inspect(Out.null()))\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "static ek_obj *ekd_show_N$std$Out(ek_val v);\n"));
    EK_CHECK(lfx_has(&f, "return ekd_inspect_N$std$Out(v);"));
    EK_CHECK(lfx_has(&f, "ek_str_from_cstr(\"<cap>\")"));
    /* the placeholder arm reads no value, and the body silences the
       parameter the way the shims silence theirs */
    EK_CHECK(lfx_has(&f, "ekd_inspect_N$std$Out(ek_val v)\n"
                         "{\n"
                         "    (void)v;\n"));
    lfx_end(&f);
}

/* The handle types answer the display channels with their own
   <handle> form, by type and not by typeinfo identity: the runtime
   builds its File values over private identities, so the branch must
   stand ahead of the variant walk. */
static void handle_display(void)
{
    lfx f;

    lfx_begin(&f,
              "main: (o: Out, fs: Fs) ! =\n"
              "    case fs.open(\"f\", Read):\n"
              "        Ok h ->\n"
              "            o.println(h.show())\n"
              "            o.println(h.inspect())\n"
              "        Err e -> o.println(\"no\")\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ek_str_from_cstr(\"<handle>\")"));
    EK_CHECK(lfx_lacks(&f, "\"<cap>\""));
    lfx_end(&f);
}

/* The function form renders <fn> on both channels, and a composite
   forwards the element's own form instead of an opaque placeholder. */
static void function_display(void)
{
    lfx f;

    lfx_begin(&f,
              "R: type = [cb: (Int) -> Int]\n"
              "V: type = Wrap ((Int) -> Int) | Done\n"
              "main: (o: Out) ! =\n"
              "    g: (Int) -> Int = (x): x\n"
              "    gs: [](Int) -> Int = {g}\n"
              "    r: R = {cb: g}\n"
              "    v: V = Wrap g\n"
              "    o.println(g.show())\n"
              "    o.println(gs.show())\n"
              "    o.println(r.show())\n"
              "    o.println(v.show())\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "static ek_obj *ekd_show_Fn(ek_val v);"));
    EK_CHECK(lfx_has(&f, "ek_str_from_cstr(\"<fn>\")"));
    EK_CHECK(lfx_lacks(&f, "ekd_inspect_Opaque"));
    lfx_end(&f);
}

/* Equality and the display channels answer over a dictionary whose
   keys are not String: the key domain gates the checker, the lowering
   derives the walk, and the keys render quoted like any other. */
static void dict_key_domain(void)
{
    lfx f;

    lfx_begin(&f,
              "main: (o: Out) ! =\n"
              "    d: [Int] Int = {1: 10, 2: 20}\n"
              "    o.println(d.show())\n"
              "    o.println(show(d.eq(d)))\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "static int ekq_D$Int$Int(ek_val a, ek_val b);"));
    EK_CHECK(lfx_has(&f, "static ek_obj *ekd_show_D$Int$Int(ek_val v);"));
    EK_CHECK(lfx_lacks(&f, "no lowering"));
    lfx_end(&f);
}

/* The decode side constructs: from_json over an algebraic type no
   source site builds registers every variant's family as constructed,
   so the helpers join the unit, and each decode arm binds the
   constructor result to its own named temp and releases it. */
static void from_json_constructs_variants(void)
{
    lfx f;

    lfx_begin(&f,
              "Shape: type = Light | Dark\n"
              "main: () -> _ =\n"
              "    j: Json = Json.parse(\"\\\"Dark\\\"\").unwrap()\n"
              "    s := Shape.from_json(j)\n"
              "    case s:\n"
              "        Ok _ -> _\n"
              "        Err _ -> _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the variant helpers stand although no source site constructs */
    EK_CHECK(lfx_has(&f, "static ek_val ekn_test$Shape$Light(void)"));
    EK_CHECK(lfx_has(&f, "static ek_val ekn_test$Shape$Dark(void)"));
    /* the decode arm builds the variant through its named temp and
       releases it once the Ok stands */
    EK_CHECK(lfx_has(&f, "ek_val q = ekn_test$Shape$Dark();\n"
                         "            ek_val w = ekn_std$Result$Ok(q);\n"
                         "            ek_release(q);\n"
                         "            return w;"));
    lfx_end(&f);
}

int main(void)
{
    specialization_naming();
    recursive_forward_decl();
    operator_routing();
    with_shape();
    ordering_and_display_members();
    tuple_payload_equality();
    capability_display();
    handle_display();
    function_display();
    dict_key_domain();
    from_json_constructs_variants();
    return ek_test_report("lower/derived");
}
