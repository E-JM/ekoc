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
    EK_CHECK(lfx_has(&f, "static int ekq_std_Option_std_Int(ek_val a, "
                         "ek_val b);\n"));
    EK_CHECK(lfx_has(&f, "static int ekq_std_Option_std_String(ek_val a, "
                         "ek_val b);\n"));
    EK_CHECK(lfx_has(&f, "ekq_std_Option_std_Int("));
    EK_CHECK(lfx_has(&f, "ekq_std_Option_std_String("));
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
    EK_CHECK(lfx_has(&f, "static ek_obj *ekd_show_test_Chain(ek_val v);\n"));
    EK_CHECK(lfx_has(&f, "static ek_obj *ekd_inspect_test_Chain(ek_val v);"
                       "\n"));
    EK_CHECK(lfx_has(&f, "ekd_inspect_test_Chain(((ekS_test_Chain_Link *)"
                       "v.as.ref)->p0)"));
    /* the dispatch tests the constructor families' typeinfo */
    EK_CHECK(lfx_has(&f, "v.as.ref->info == &ekT_test_Chain_End"));
    EK_CHECK(lfx_has(&f, "v.as.ref->info == &ekT_test_Chain_Link"));
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
    EK_CHECK(lfx_has(&f, "ek_bool(ekq_test_Tree("));
    EK_CHECK(lfx_has(&f, "ek_bool(!ekq_test_Tree("));
    EK_CHECK(lfx_has(&f, "ek_bool(eko_std_String("));
    EK_CHECK(lfx_has(&f, "ek_bool(ekq_std_String("));
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
    EK_CHECK(lfx_has(&f, "ekn_test_Money_rec(t"));
    EK_CHECK(lfx_has(&f, "ek_keep(((ekS_test_Money *)t"));
    EK_CHECK(lfx_has(&f, ")->f_cur)"));
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
    EK_CHECK(lfx_has(&f, "ek_bool(eko_test_Sex("));
    EK_CHECK(lfx_has(&f, "ekd_show_std_Int(ek_val v)"));
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
    EK_CHECK(lfx_has(&f, "static ek_obj *ekd_show_std_Out(ek_val v);\n"));
    EK_CHECK(lfx_has(&f, "return ekd_inspect_std_Out(v);"));
    EK_CHECK(lfx_has(&f, "ek_str_from_cstr(\"<cap>\")"));
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
    return ek_test_report("lower/derived");
}
