#include "common.h"

/* The derived display and JSON services: show and inspect carry a real
   (t) -> String arrow over a determined receiver, and the JSON pair
   carries (t) -> Json and (Json) -> Result t ParseError. The free and
   receiver spellings reach the same derived member; a declared
   namespace member wins over show's default; the JSON mapping halts at
   a capability, a function, and a non-string-keyed dictionary with
   E-json-shape. */

/* Any check row on a source line whose type is the nominal of the
   module and name. */
static int row_nominal_at(const cfx *f, int line, const char *module,
                          const char *name)
{
    size_t i;

    for (i = 0; i < f->chk.count; i++) {
        const ek_type *t = f->chk.rows[i].type;

        if (!f->chk.rows[i].node || f->chk.rows[i].node->line != line
            || !t || t->kind != EK_T_NOMINAL || !t->u.nominal.sym) {
            continue;
        }
        if (strcmp(t->u.nominal.sym->module, module) == 0
            && strcmp(t->u.nominal.sym->name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

/* The first row on a line whose node has the kind. */
static const ek_ck_row *row_of_kind(const cfx *f, int line, ek_node_kind k)
{
    size_t i;

    for (i = 0; i < f->chk.count; i++) {
        if (f->chk.rows[i].node && f->chk.rows[i].node->line == line
            && f->chk.rows[i].node->kind == k) {
            return &f->chk.rows[i];
        }
    }
    return NULL;
}

/* A declared record and ADT: show and inspect derive the real String
   arrow in both spellings, and the qualified spelling takes no
   receiver. */
static void display_on_declared(void)
{
    cfx f;

    cfx_begin(&f,
              "Sex: type = Man | Woman\n"
              "Tree: type t = Leaf t | Node [Tree t, Tree t]\n"
              "Money: type = [cents: Int, cur: String]\n"
              "\n"
              "m: Money = {cents: 1, cur: \"USD\"}\n"
              "t: Tree Int = Node {Leaf 1, Leaf 2}\n"
              "s1: String = show(m)\n"
              "s2: String = m.show()\n"
              "s3: String = Money.show(m)\n"
              "s4: String = inspect(m)\n"
              "s5: String = show(t)\n"
              "s6: String = t.inspect()\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 7, EK_RD_DERIVED));
    EK_CHECK(cfx_reading_at(&f, 8, EK_RD_DERIVED));
    EK_CHECK(cfx_reading_at(&f, 9, EK_RD_DERIVED));
    EK_CHECK(cfx_reading_at(&f, 10, EK_RD_DERIVED));
    EK_CHECK(row_nominal_at(&f, 7, "std", "String"));
    EK_CHECK(row_nominal_at(&f, 8, "std", "String"));
    EK_CHECK(row_nominal_at(&f, 9, "std", "String"));
    EK_CHECK(row_nominal_at(&f, 10, "std", "String"));
    EK_CHECK(row_nominal_at(&f, 11, "std", "String"));
    EK_CHECK(row_nominal_at(&f, 12, "std", "String"));
    cfx_end(&f);
}

/* The core types are declared, so their display services derive too. */
static void display_on_core_types(void)
{
    cfx f;

    cfx_begin(&f,
              "o: Option Int = Some 1\n"
              "r: Result Int String = Ok 3\n"
              "b: Bool = True\n"
              "s1: String = show(o)\n"
              "s2: String = o.inspect()\n"
              "s3: String = inspect(r)\n"
              "s4: String = show(b)\n"
              "s5: String = Option.show(o)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 4, EK_RD_DERIVED));
    EK_CHECK(cfx_reading_at(&f, 5, EK_RD_DERIVED));
    EK_CHECK(cfx_reading_at(&f, 6, EK_RD_DERIVED));
    EK_CHECK(cfx_reading_at(&f, 7, EK_RD_DERIVED));
    EK_CHECK(cfx_reading_at(&f, 8, EK_RD_DERIVED));
    EK_CHECK(row_nominal_at(&f, 4, "std", "String"));
    EK_CHECK(row_nominal_at(&f, 7, "std", "String"));
    cfx_end(&f);
}

/* Structural composites display: a tuple, a list, a string-keyed
   dictionary, and the unit type are determined receivers. */
static void display_on_composites(void)
{
    cfx f;

    cfx_begin(&f,
              "t: [Int, String] = {1, \"a\"}\n"
              "xs: []Int = {1, 2}\n"
              "d: [String]Int = {\"a\": 1}\n"
              "s1: String = show(t)\n"
              "s2: String = show(xs)\n"
              "s3: String = inspect(d)\n"
              "s4: String = show(_)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 4, EK_RD_DERIVED));
    EK_CHECK(cfx_reading_at(&f, 5, EK_RD_DERIVED));
    EK_CHECK(cfx_reading_at(&f, 6, EK_RD_DERIVED));
    EK_CHECK(row_nominal_at(&f, 4, "std", "String"));
    cfx_end(&f);
}

/* The display services derive over every determined receiver, the
   scalar and capability placeholders included: the arrow is real and
   pure even though the type's shape is still opaque. */
static void display_placeholder_opacity(void)
{
    cfx f;

    cfx_begin(&f,
              "v: String = \"hi\"\n"
              "n: Int = 1\n"
              "a: String = show(v)\n"
              "b: String = v.inspect()\n"
              "c: String = show(n)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 3, EK_RD_DERIVED));
    EK_CHECK(cfx_reading_at(&f, 4, EK_RD_DERIVED));
    EK_CHECK(cfx_reading_at(&f, 5, EK_RD_DERIVED));
    cfx_end(&f);
}

/* A function receiver displays as a value; its JSON form halts like any
   other function. */
static void display_on_functions(void)
{
    cfx f;

    cfx_begin(&f,
              "g: (n: Int) -> Int = n\n"
              "s1: String = show(g)\n"
              "s2: String = g.inspect()\n"
              "j: Json = g.to_json()\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-json-shape", 4));
    EK_CHECK(cfx_reading_at(&f, 2, EK_RD_DERIVED));
    EK_CHECK(cfx_reading_at(&f, 3, EK_RD_DERIVED));
    EK_CHECK(row_nominal_at(&f, 2, "std", "String"));
    cfx_end(&f);

    /* Another member of a function value stays unbound. */
    cfx_begin(&f,
              "g: (n: Int) -> Int = n\n"
              "v: Int = g.bogus\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 2));
    cfx_end(&f);
}

/* The declaring module's show wins over the default; inspect beside it
   is still the derived one. */
static void declared_show_wins(void)
{
    cfx f;

    cfx_begin(&f,
              "Money: type = [cents: Int, cur: String]\n"
              "Money.show: (m: Money) -> String = \"{m.cur} {m.cents}\"\n"
              "m: Money = {cents: 12500, cur: \"USD\"}\n"
              "a: String = show(m)\n"
              "b: String = m.show()\n"
              "c: String = inspect(m)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 4, EK_RD_NAMESPACE));
    EK_CHECK(cfx_reading_at(&f, 5, EK_RD_NAMESPACE));
    EK_CHECK(cfx_reading_at(&f, 6, EK_RD_DERIVED));
    cfx_end(&f);
}

/* The JSON pair derives over a record and an ADT: to_json yields the
   placeholder Json, and the type-headed from_json yields
   Result t ParseError. */
static void json_derived(void)
{
    cfx f;
    const ek_ck_row *row;

    cfx_begin(&f,
              "Sex: type = Man | Woman\n"
              "Profile: type = [name: String, sex: Sex]\n"
              "\n"
              "p: Profile = {name: \"ada\", sex: Woman}\n"
              "man: Sex = Man\n"
              "j1: Json = p.to_json()\n"
              "j2: Json = Sex.to_json(man)\n"
              "r1: Result Profile ParseError = Profile.from_json(j1)\n"
              "r2: Result Sex ParseError = Sex.from_json(j2)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 6, EK_RD_DERIVED));
    EK_CHECK(cfx_reading_at(&f, 7, EK_RD_DERIVED));
    EK_CHECK(cfx_reading_at(&f, 8, EK_RD_DERIVED));
    EK_CHECK(row_nominal_at(&f, 6, "std", "Json"));
    row = row_of_kind(&f, 8, EK_N_CALL);
    EK_CHECK(row != NULL && row->type && row->type->kind == EK_T_NOMINAL
             && row->type->u.nominal.sym
             && strcmp(row->type->u.nominal.sym->module, "std") == 0
             && strcmp(row->type->u.nominal.sym->name, "Result") == 0
             && row->type->u.nominal.argc == 2
             && row->type->u.nominal.args[0]->kind == EK_T_NOMINAL
             && strcmp(row->type->u.nominal.args[0]->u.nominal.sym->name,
                       "Profile") == 0
             && row->type->u.nominal.args[1]->kind == EK_T_NOMINAL
             && strcmp(row->type->u.nominal.args[1]->u.nominal.sym->name,
                       "ParseError") == 0);
    cfx_end(&f);
}

/* The core types and the structural composites map as well. */
static void json_core_and_composites(void)
{
    cfx f;

    cfx_begin(&f,
              "o: Option Int = Some 1\n"
              "r: Result Int String = Ok 3\n"
              "xs: []Int = {1, 2}\n"
              "t: [Int, String] = {1, \"a\"}\n"
              "d: [String]Int = {\"a\": 1}\n"
              "n: Int = 1\n"
              "j1: Json = o.to_json()\n"
              "j2: Json = r.to_json()\n"
              "j3: Json = xs.to_json()\n"
              "j4: Json = t.to_json()\n"
              "j5: Json = d.to_json()\n"
              "j6: Json = n.to_json()\n"
              "b1: Result (Option Int) ParseError = Option.from_json(j1)\n"
              "b2: Result (Result Int String) ParseError = Result.from_json(j2)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(row_nominal_at(&f, 7, "std", "Json"));
    EK_CHECK(row_nominal_at(&f, 12, "std", "Json"));
    cfx_end(&f);
}

/* The JSON mapping halts with E-json-shape at a capability, a function,
   and a non-string-keyed dictionary, directly and through a field. */
static void json_halts(void)
{
    cfx f;

    /* A capability receiver carries no JSON form; the form halts, and
       the call itself is pure. */
    cfx_begin(&f,
              "f: (cb: Out) -> Json = cb.to_json()\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-json-shape", 1));
    cfx_end(&f);

    cfx_begin(&f,
              "g: () -> Json =\n"
              "    d: [Int]String = {1: \"a\"}\n"
              "    d.to_json()\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-json-shape", 3));
    cfx_end(&f);

    cfx_begin(&f,
              "Cap: type = [cb: Out]\n"
              "FnT: type = [f: (Int) -> Int]\n"
              "Kd: type = [d: [Int]String]\n"
              "\n"
              "c: (out: Out) -> Json =\n"
              "    cap: Cap = {cb: out}\n"
              "    cap.to_json()\n"
              "g: (n: Int) -> Int = n\n"
              "v: FnT = {f: g}\n"
              "w: Json = v.to_json()\n"
              "k: Kd = {d: {1: \"a\"}}\n"
              "x: Json = k.to_json()\n");
    EK_CHECK_U64(cfx_errors(&f), 3);
    EK_CHECK(cfx_code_at_line(&f, "E-json-shape", 7));
    EK_CHECK(cfx_code_at_line(&f, "E-json-shape", 10));
    EK_CHECK(cfx_code_at_line(&f, "E-json-shape", 12));
    cfx_end(&f);

    /* The display services never halt: a capability field shows. */
    cfx_begin(&f,
              "Cap: type = [cb: Out]\n"
              "f: (out: Out) -> String =\n"
              "    c: Cap = {cb: out}\n"
              "    c.show()\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 4, EK_RD_DERIVED));
    cfx_end(&f);
}

/* The equality pair and the ordering family derive over the value layer
   and declared ADTs; capabilities and functions carry neither. */
static void equality_and_order_families(void)
{
    cfx f;

    cfx_begin(&f,
              "a: Int = 1\n"
              "b: Int = 2\n"
              "n: Bool = a.ne(b)\n"
              "l: Bool = a.le(b)\n"
              "g: Bool = a.gt(b)\n"
              "e: Bool = a.ge(b)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 3, EK_RD_DERIVED));
    EK_CHECK(cfx_reading_at(&f, 6, EK_RD_DERIVED));
    cfx_end(&f);

    cfx_begin(&f,
              "Sex: type = Man | Woman\n"
              "x: Sex = Man\n"
              "y: Sex = Woman\n"
              "a: Bool = x.ne(y)\n"
              "b: Bool = x.le(y)\n"
              "c: Bool = x.gt(y)\n"
              "d: Bool = x.ge(y)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 4, EK_RD_DERIVED));
    cfx_end(&f);

    /* A capability has no equality and no ordering. */
    cfx_begin(&f, "f: (a: Out, b: Out) -> Bool = a.ne(b)\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-no-eq", 1));
    cfx_end(&f);

    cfx_begin(&f, "f: (a: Out, b: Out) -> Bool = a.lt(b)\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-no-eq", 1));
    cfx_end(&f);

    /* A function has none either, whichever member names it. */
    cfx_begin(&f,
              "g: (n: Int) -> Int = n\n"
              "h: (n: Int) -> Int = n\n"
              "b: Bool = h.ne(g)\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-no-eq", 3));
    cfx_end(&f);

    /* Display over a capability is pure, so a pure arrow admits it. */
    cfx_begin(&f,
              "s: (cap: Out) -> String = show(cap)\n"
              "t: (cap: Out) -> String = cap.inspect()\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 1, EK_RD_DERIVED));
    cfx_end(&f);
}

int main(void)
{
    display_on_declared();
    display_on_core_types();
    display_on_composites();
    display_placeholder_opacity();
    display_on_functions();
    declared_show_wins();
    json_derived();
    json_core_and_composites();
    json_halts();
    equality_and_order_families();
    return ek_test_report("check/derived");
}
