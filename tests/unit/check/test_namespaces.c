#include "common.h"

/* The compiler-provided rows of the namespace table: the boolean
   negation, the List signatures over the structural list, the
   dictionary and set containers, the string and byte families, the
   Option and Result combinators, ranges, the time and duration types,
   JSON, paths, the scalar conversion, formatting, and machine-word
   rows, and the Out capability rows, both spellings, and the closed
   namespaces — a name no row carries is absent, not bridged. */

static void list_rows(void)
{
    cfx f;

    cfx_begin(&f,
              "xs: []Int = {1, 2, 3}\n"
              "n: Int = xs.length()\n"
              "g: Option Int = xs.get(0)\n"
              "t: []Int = xs.take(2)\n"
              "d: []Int = xs.drop(1)\n"
              "r: []Int = xs.reverse().append(xs)\n"
              "j: String = xs.join(\"-\")\n"
              "s: Option ([]Int) = xs.slice(0, 2)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "xs: []Int = {1, 2, 3}\n"
              "d: []Int = xs.map((x): x * 2)\n"
              "w: []Int = xs.filter((x): x > 1).sort()\n"
              "z: Int = xs.fold(0, (acc, x): acc + x)\n"
              "e: Bool = xs.any((x): x > 2)\n"
              "a: Bool = xs.all((x): x > 0)\n"
              "o: Option Int = xs.find_by((x): x > 1)\n"
              "b: []Int = xs.sort_by((p, q): p < q)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* The type-namespace spelling carries the receiver as its first
       argument, and the chain head resolves through the List row. */
    cfx_begin(&f,
              "xs: []Int = {1, 2, 3}\n"
              "ys: []Int = List.map(xs, (x): x * 2)\n"
              "n: Int = List.length(xs)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 2, EK_RD_NAMESPACE));
    cfx_end(&f);
}

static void list_rows_types(void)
{
    cfx f;

    /* A row's result type is checked at the binding: map over Int
       does not land where []String is expected. */
    cfx_begin(&f,
              "xs: []Int = {1, 2, 3}\n"
              "ys: []String = xs.map((x): x * 2)\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-type-mismatch", 2));
    cfx_end(&f);

    /* The receiver binds the element type first, so the callback's
       parameter is concrete: the argument mismatch and the result
       mismatch are both reported. */
    cfx_begin(&f,
              "xs: []Int = {1, 2, 3}\n"
              "ys: []Int = xs.map((x): x.eq(\"no\"))\n");
    EK_CHECK_U64(cfx_errors(&f), 2);
    EK_CHECK(cfx_code_at_line(&f, "E-nominal-mismatch", 2));
    EK_CHECK(cfx_code_at_line(&f, "E-type-mismatch", 2));
    cfx_end(&f);

    /* Arity is the row's. */
    cfx_begin(&f,
              "xs: []Int = {1, 2, 3}\n"
              "n: Int = xs.length()\n"
              "m: Int = xs.length(1)\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-call-arity", 3));
    cfx_end(&f);
}

static void list_namespace_closed(void)
{
    cfx f;

    cfx_begin(&f,
              "xs: []Int = {1, 2, 3}\n"
              "v: Int = xs.bogus()\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 2));
    cfx_end(&f);

    cfx_begin(&f,
              "xs: []Int = {1, 2, 3}\n"
              "v: Int = List.bogus(xs)\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 2));
    cfx_end(&f);

    /* The derived services still derive over a list receiver; eq is
       structural, and the bogus miss is reported for it too. */
    cfx_begin(&f,
              "xs: []Int = {1, 2, 3}\n"
              "ys: []Int = {1, 2, 3}\n"
              "e: Bool = xs.eq(ys)\n"
              "v: Int = ys.inspect()\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-nominal-mismatch", 4));
    cfx_end(&f);
}

static void bool_rows(void)
{
    cfx f;

    /* The one non-derived member, in both spellings. */
    cfx_begin(&f,
              "b: Bool = True\n"
              "v: Bool = b.not()\n"
              "w: Bool = Bool.not(b)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 2, EK_RD_NAMESPACE));
    cfx_end(&f);

    /* The row types the result: the binding rejects a non-Bool. */
    cfx_begin(&f,
              "b: Bool = True\n"
              "n: Int = b.not()\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-nominal-mismatch", 2));
    cfx_end(&f);

    /* The row checks the argument. */
    cfx_begin(&f,
              "b: Bool = True\n"
              "v: Bool = b.not(1)\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    cfx_end(&f);

    cfx_begin(&f,
              "b: Bool = True\n"
              "v: Bool = b.not(\"x\")\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    cfx_end(&f);

    /* The namespace is complete: a name no row carries is absent, not
       bridged. */
    cfx_begin(&f,
              "b: Bool = True\n"
              "v: Bool = b.bogus()\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 2));
    cfx_end(&f);
}

static void ints_rows(void)
{
    cfx f;

    /* Both spellings, a mixed operand type, and a constant member. */
    cfx_begin(&f,
              "a: Int = 1.add(2)\n"
              "b: Int = Int.sub(10, 4)\n"
              "c: Int = 2.pow(10)\n"
              "d: Int = Int.min\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* The row's result lands at the binding. */
    cfx_begin(&f, "a: String = 1.add(2)\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-nominal-mismatch", 1));
    cfx_end(&f);

    /* The row's parameter rejects the wrong operand type. */
    cfx_begin(&f, "a: Int = 1.add(\"x\")\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-nominal-mismatch", 1));
    cfx_end(&f);

    /* The scalar namespaces are complete, so a name no row carries is
       absent. */
    cfx_begin(&f, "v: Int = Int.bogus(1)\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 1));
    cfx_end(&f);

    /* A constant member is a value, not a function. */
    cfx_begin(&f, "v: Int = Int.min()\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-not-callable", 1));
    cfx_end(&f);

    /* Unsigned values have no negation and the namespace is closed. */
    cfx_begin(&f, "v: UInt = UInt.neg(1)\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 1));
    cfx_end(&f);

    /* A from_ conversion exists only where the machine domain fits:
       U64 does not fit Int, so the member is absent. */
    cfx_begin(&f,
              "u: U64 = 1\n"
              "v: Int = Int.from_u64(u)\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 2));
    cfx_end(&f);

    /* The unsigned word keeps the total widening, and the included
       signed domains still widen into Int. */
    cfx_begin(&f,
              "u: U64 = 1\n"
              "w: U32 = 1\n"
              "v: UInt = UInt.from_u64(u)\n"
              "n: Int = Int.from_u32(w)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

static void scalar_completion(void)
{
    cfx f;

    /* The scalar namespaces carry their conversion, formatting, and
       machine-word rows; each name resolves off its namespace. */
    cfx_begin(&f,
              "n: String = 255.to_hex()\n"
              "m: Result Int ParseError = Int.parse(\"12\")\n"
              "o: Option Int = 3.5.trunc().to_int()\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f, "v: Bool = (0.0 / 0.0).is_nan()\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f, "w: Option U8 = 255.to_u8()\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* Arity is the row's; the binding matches the row's result, so
       the surplus argument is the only diagnostic. */
    cfx_begin(&f, "v: String = 255.to_hex(1)\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-call-arity", 1));
    cfx_end(&f);

    /* The parse family takes a String; the binding matches the row's
       result, so the integer argument is the only diagnostic. */
    cfx_begin(&f, "v: Result Int ParseError = Int.parse(12)\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    cfx_end(&f);
}

static void derivation_rows(void)
{
    cfx f;

    /* The advancing derivations land on the std placeholders, so the
       annotated bindings conform. */
    cfx_begin(&f,
              "i: In = In.from_lines({\"one\"})\n"
              "r: Rand = Rand.with_seed(7)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* The unannotated result carries the namespace: a foreign anchor
       is the mismatch, not a silent variable. */
    cfx_begin(&f, "n: Int = In.from_lines({\"one\"})\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-nominal-mismatch", 1));
    cfx_end(&f);

    cfx_begin(&f, "n: Int = Rand.with_seed(7)\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-nominal-mismatch", 1));
    cfx_end(&f);

    /* The namespace rides the chain unannotated: read_line checks, its
       world arrow demands the bit in a pure binding, and its result
       mismatches the anchor. */
    cfx_begin(&f,
              "s: String = In.from_lines({\"one\"}).read_line()\n");
    EK_CHECK_U64(cfx_errors(&f), 2);
    EK_CHECK(cfx_code_at_line(&f, "E-nominal-mismatch", 1));
    EK_CHECK(cfx_code_at_line(&f, "E-world-bit", 1));
    cfx_end(&f);

    /* In a world arrow the chain simply types: the body's result is
       read_line's, and the anchor rejects it. */
    cfx_begin(&f,
              "go: (l: []String) !-> Int =\n"
              "    In.from_lines(l).read_line()\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-nominal-mismatch", 2));
    cfx_end(&f);

    /* The chain head's namespace is closed, so an absent member is the
       miss, not the bridge. */
    cfx_begin(&f,
              "v: Int = In.from_lines({\"one\"}).bogus()\n");
    EK_CHECK_U64(cfx_errors(&f), 2);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 1));
    EK_CHECK(cfx_code_at_line(&f, "E-world-bit", 1));
    cfx_end(&f);
}

static void out_rows(void)
{
    cfx f;

    /* Printing touches the world, so a pure arrow cannot carry it. */
    cfx_begin(&f,
              "log: (out: Out) -> _ =\n"
              "    out.println(\"x\")\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-world-bit", 1));
    cfx_end(&f);

    /* A world arrow admits the print, in both spellings. */
    cfx_begin(&f,
              "log: (out: Out) !-> _ =\n"
              "    out.println(\"x\")\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "log: (out: Out) !-> _ =\n"
              "    Out.println(out, \"x\")\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* null is a pure derivation. */
    cfx_begin(&f,
              "cap: () -> Out =\n"
              "    Out.null()\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* tee observes through a world arrow but derives purely. */
    cfx_begin(&f,
              "t: (out: Out, obs: (String) !-> _) -> Out =\n"
              "    out.tee(obs)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* The namespace is closed; the absent
       member leaves the body pure, so the surplus `!` is the second
       diagnostic. */
    cfx_begin(&f,
              "log: (out: Out) !-> Int =\n"
              "    v: Int = out.bogus()\n");
    EK_CHECK_U64(cfx_errors(&f), 2);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 2));
    EK_CHECK(cfx_code_at_line(&f, "E-world-bit", 2));
    cfx_end(&f);
}

static void dict_rows(void)
{
    cfx f;

    /* The dictionary rows over the structural receiver, including the
       type-namespace spelling and both open callbacks. */
    cfx_begin(&f,
              "d: [String]Int = {\"a\": 1}\n"
              "n: Int = d.length()\n"
              "g: Option Int = d.get(\"a\")\n"
              "i: [String]Int = Dict.insert(d, \"b\", 2)\n"
              "r: [String]Int = d.remove(\"a\")\n"
              "m: [String]Int = d.merge(d)\n"
              "mp: [String]String = d.map((k, v): k.append(v.to_hex()))\n"
              "fl: [String]Int = d.filter((k, v): v > 0)\n"
              "z: Int = d.fold(0, (acc, k, v): acc + v)\n"
              "e: _ = d.each((k, v): _)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "d: [String]Int = {\"a\": 1}\n"
              "v: Int = d.bogus()\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 2));
    cfx_end(&f);

    cfx_begin(&f,
              "d: [String]Int = {\"a\": 1}\n"
              "g: Option Int = d.get(1)\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    cfx_end(&f);
}

static void set_rows(void)
{
    cfx f;

    cfx_begin(&f,
              "s: Set Int = Set.of({1, 2})\n"
              "xs: []Int = s.to_list()\n"
              "b: Bool = s.contains(1)\n"
              "i: Set Int = s.insert(3)\n"
              "r: Set Int = s.remove(1)\n"
              "n: Int = s.length()\n"
              "u: Set Int = s.union(s)\n"
              "x: Set Int = s.intersect(s)\n"
              "d: Set Int = s.diff(s)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "s: Set Int = Set.of({1, 2})\n"
              "v: Int = s.bogus()\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 2));
    cfx_end(&f);

    cfx_begin(&f,
              "s: Set Int = Set.of({1, 2})\n"
              "b: Bool = s.contains(\"x\")\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    cfx_end(&f);
}

static void string_rows(void)
{
    cfx f;

    cfx_begin(&f,
              "s: String = \"a,b\"\n"
              "n: Int = s.length()\n"
              "c: Option Char = s.at(0)\n"
              "cs: []Char = s.chars()\n"
              "f: String = String.from_char('x')\n"
              "g: String = String.from_chars(cs)\n"
              "a: String = s.append(\"!\")\n"
              "l: []String = s.lines()\n"
              "p: []String = s.split(\",\")\n"
              "h: Bool = s.contains(\",\")\n"
              "i: Bool = s.starts_with(\"a\")\n"
              "j: Bool = s.ends_with(\"b\")\n"
              "r: String = s.replace(\",\", \";\")\n"
              "r1: String = s.replace_first(\",\", \";\")\n"
              "sl: Option String = s.slice(0, 1)\n"
              "t: String = s.trim()\n"
              "ts: String = s.trim_start()\n"
              "te: String = s.trim_end()\n"
              "b: Bytes = s.to_bytes()\n"
              "pl: String = s.pad_left(4, '0')\n"
              "pr: String = s.pad_right(4, '0')\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "s: String = \"x\"\n"
              "v: Int = s.bogus()\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 2));
    cfx_end(&f);

    cfx_begin(&f,
              "s: String = \"x\"\n"
              "n: Int = s.length(1)\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-call-arity", 2));
    cfx_end(&f);
}

static void char_rows(void)
{
    cfx f;

    cfx_begin(&f,
              "c: Char = 'a'\n"
              "n: Int = c.to_code()\n"
              "o: Option Char = Char.from_code(97)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "c: Char = 'a'\n"
              "v: Int = c.bogus()\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 2));
    cfx_end(&f);

    cfx_begin(&f,
              "v: Option Char = Char.from_code(\"x\")\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    cfx_end(&f);
}

static void bytes_rows(void)
{
    cfx f;

    cfx_begin(&f,
              "b: Bytes = \"ab\".to_bytes()\n"
              "n: Int = b.length()\n"
              "g: Option Int = b.get(0)\n"
              "c: Bytes = b.append(b)\n"
              "is: []Int = b.to_ints()\n"
              "fi: Option Bytes = Bytes.from_ints(is)\n"
              "us: []U8 = b.to_u8s()\n"
              "fu: Option Bytes = Bytes.from_u8s(us)\n"
              "h: String = b.to_hex()\n"
              "fh: Result Bytes ParseError = Bytes.from_hex(\"00\")\n"
              "s: Result String ParseError = b.to_string()\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "b: Bytes = \"ab\".to_bytes()\n"
              "v: Int = b.bogus()\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 2));
    cfx_end(&f);

    cfx_begin(&f,
              "b: Bytes = \"ab\".to_bytes()\n"
              "v: Int = b.get(0)\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    cfx_end(&f);
}

static void option_rows(void)
{
    cfx f;

    cfx_begin(&f,
              "x: Option Int = Some 5\n"
              "m: Option String = x.map((n): n.to_hex())\n"
              "a: Option Int = x.and_then((n): Some n)\n"
              "o: Option Int = x.or_else((): Some 0)\n"
              "f: Option Int = x.filter((n): n > 0)\n"
              "s: Bool = x.is_some()\n"
              "n: Bool = x.is_none()\n"
              "u: Int = x.unwrap()\n"
              "uo: Int = x.unwrap_or(0)\n"
              "e: Int = x.expect(\"none\")\n"
              "grid: Option ([]Int) = Some {1, 2}\n"
              "gi: Option Int = grid.get(0)\n"
              "r: Result Int String = x.to_result(\"none\")\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "x: Option Int = Some 5\n"
              "v: Int = x.bogus()\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 2));
    cfx_end(&f);

    cfx_begin(&f,
              "x: Option Int = Some 5\n"
              "u: Int = x.unwrap_or(\"x\")\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    cfx_end(&f);
}

static void result_rows(void)
{
    cfx f;

    cfx_begin(&f,
              "r: Result Int String = Ok 5\n"
              "t: Result String String = r.try((n): Ok n.to_hex())\n"
              "m: Result String String = r.map((n): n.to_hex())\n"
              "e: Result Int Int = r.map_err((s): s.length())\n"
              "tr: Result ([]String) String = Result.traverse({1, 2}, (n): Ok n.to_hex())\n"
              "ok: Bool = r.is_ok()\n"
              "er: Bool = r.is_err()\n"
              "u: Int = r.unwrap()\n"
              "uo: Int = r.unwrap_or(0)\n"
              "x: Int = r.expect(\"no\")\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "r: Result Int String = Ok 5\n"
              "v: Int = r.bogus()\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 2));
    cfx_end(&f);

    cfx_begin(&f,
              "r: Result Int String = Ok 5\n"
              "v: Result String String = r.map((n): n)\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    cfx_end(&f);
}

static void range_rows(void)
{
    cfx f;

    cfx_begin(&f,
              "r: Range Int = 1..9\n"
              "n: Int = r.length()\n"
              "xs: []Int = r.to_list()\n"
              "ys: []Int = r.map((v): v * 2)\n"
              "z: Int = r.fold(0, (acc, v): acc + v)\n"
              "e: _ = r.each((v): _)\n"
              "ei: _ = r.each_with_index((v, i): _)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "r: Range Int = 1..9\n"
              "v: Int = r.bogus()\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 2));
    cfx_end(&f);

    cfx_begin(&f,
              "r: Range Int = 1..9\n"
              "ys: []String = r.map((v): v * 2)\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    cfx_end(&f);
}

static void time_rows(void)
{
    cfx f;

    cfx_begin(&f,
              "t: Time = Time 5\n"
              "d: Duration = t.sub(Time 2)\n"
              "u: Time = t.add(Duration 3)\n"
              "n: Int = t.to_ns()\n"
              "p: Result Time ParseError = Time.parse(\"1970-01-01T00:00:00Z\")\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "t: Time = Time 5\n"
              "v: Int = t.bogus()\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 2));
    cfx_end(&f);

    cfx_begin(&f,
              "t: Time = Time 5\n"
              "v: Time = t.add(Time 2)\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    cfx_end(&f);
}

static void mono_rows(void)
{
    cfx f;

    cfx_begin(&f,
              "m: Mono = Mono 5\n"
              "d: Duration = m.sub(Mono 2)\n"
              "u: Mono = m.add(Duration 3)\n"
              "n: Int = m.to_ns()\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "m: Mono = Mono 5\n"
              "v: Int = m.bogus()\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 2));
    cfx_end(&f);

    cfx_begin(&f,
              "m: Mono = Mono 5\n"
              "v: Mono = m.add(Mono 2)\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    cfx_end(&f);
}

static void duration_rows(void)
{
    cfx f;

    cfx_begin(&f,
              "a: Duration = Duration.secs(2)\n"
              "b: Duration = Duration.millis(500)\n"
              "c: Duration = a.add(b)\n"
              "d: Duration = c.mul(3)\n"
              "e: Duration = d.neg()\n"
              "n: Int = e.to_ns()\n"
              "p: Result Duration ParseError = Duration.parse(\"1.5\")\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "a: Duration = Duration.secs(2)\n"
              "v: Int = a.bogus()\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 2));
    cfx_end(&f);

    cfx_begin(&f,
              "a: Duration = Duration.secs(2)\n"
              "v: Duration = a.mul(\"x\")\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    cfx_end(&f);
}

static void json_rows(void)
{
    cfx f;

    /* parse is the only namespace row; show stays the derived
       service, so the two coexist over the same head. */
    cfx_begin(&f,
              "j: Json = Json.parse(\"null\").unwrap()\n"
              "s: String = j.show()\n"
              "p: Result Json ParseError = Json.parse(\"[]\")\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "v: Result Json ParseError = Json.bogus(\"x\")\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 1));
    cfx_end(&f);

    cfx_begin(&f,
              "v: Result Json ParseError = Json.parse(1)\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    cfx_end(&f);
}

static void path_rows(void)
{
    cfx f;

    cfx_begin(&f,
              "j: String = Path.join(\"a\", \"b\")\n"
              "n: String = Path.normalize(\"a/./b\")\n"
              "a: Bool = Path.is_absolute(\"/x\")\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f, "v: String = Path.bogus(\"x\")\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 1));
    cfx_end(&f);

    cfx_begin(&f, "v: Bool = Path.is_absolute(1)\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    cfx_end(&f);
}

static void machine_words(void)
{
    cfx f;

    /* Every machine word carries the padding row; the comparison and
       show members stay derived over the same receiver. */
    cfx_begin(&f,
              "i8: I8 = 1\n"
              "i16: I16 = 1\n"
              "i32: I32 = 1\n"
              "i64: I64 = 1\n"
              "u8: U8 = 1\n"
              "u16: U16 = 1\n"
              "u32: U32 = 1\n"
              "u64: U64 = 1\n"
              "f32: F32 = 1.5.to_f32()\n"
              "f64: F64 = 1.5.to_f64()\n"
              "a: String = i8.to_hex_pad(2)\n"
              "b: String = i16.to_hex_pad(4)\n"
              "c: String = i32.to_hex_pad(8)\n"
              "d: String = i64.to_hex_pad(16)\n"
              "e: String = u8.to_hex_pad(2)\n"
              "g: String = u16.to_hex_pad(4)\n"
              "h: String = u32.to_hex_pad(8)\n"
              "i: String = u64.to_hex_pad(16)\n"
              "j: String = f32.to_hex_pad(8)\n"
              "k: String = f64.to_hex_pad(16)\n"
              "l: Bool = i8.eq(1)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "a: I8 = 1\n"
              "v: Int = a.bogus()\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 2));
    cfx_end(&f);

    cfx_begin(&f,
              "a: I8 = 1\n"
              "v: String = a.to_hex_pad(\"x\")\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    cfx_end(&f);
}

int main(void)
{
    list_rows();
    list_rows_types();
    list_namespace_closed();
    bool_rows();
    ints_rows();
    scalar_completion();
    derivation_rows();
    out_rows();
    dict_rows();
    set_rows();
    string_rows();
    char_rows();
    bytes_rows();
    option_rows();
    result_rows();
    range_rows();
    time_rows();
    mono_rows();
    duration_rows();
    json_rows();
    path_rows();
    machine_words();
    return ek_test_report("check/namespaces");
}
