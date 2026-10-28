#include "common.h"

/* The shipped std surface: the members the std source declares, over
   the compiler's intrinsic rows. The fixture type-checks one use of
   each family, so a member that loses its declaration falls through to
   the intrinsic table (or fails) instead of disappearing. */

static void list_surface(void)
{
    cfx f;

    cfx_begin(&f,
              "xs: []Int = {3, 1, 2}\n"
              "\n"
              "a: Int = xs.length()\n"
              "b: Option Int = xs.get(1)\n"
              "c: []Int = xs.reverse()\n"
              "d: []Int = xs ++ {4}\n"
              "e: []Int = xs.map((v): v + 1)\n"
              "f: []Int = xs.filter((v): v > 1)\n"
              "g: Int = xs.fold(0, (acc, v): acc + v)\n"
              "h: _ = xs.each((v): _)\n"
              "i: _ = xs.each_with_index((v, k): _)\n"
              "j: Bool = xs.any((v): v == 2)\n"
              "k: Bool = xs.all((v): v > 0)\n"
              "l: Option Int = xs.find_by((v): v == 2)\n"
              "m: []Int = xs.sort()\n"
              "n: []Int = xs.sort_by((x, y): x < y)\n"
              "o: []Int = List.zip_with(xs, xs, (x, y): x + y)\n"
              "p: []Int = xs.take(2)\n"
              "q: []Int = xs.drop(2)\n"
              "r: []Int = xs.take_while((v): v > 0)\n"
              "s: []Int = xs.drop_while((v): v > 0)\n"
              "t: String = xs.join(\",\")\n"
              "u: Option ([]Int) = xs.slice(1, 2)\n"
              "v: []Int = List.append(xs, xs)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

static void option_result_surface(void)
{
    cfx f;

    cfx_begin(&f,
              "xs: []Int = {1, 2}\n"
              "oo: Option Int = Some 1\n"
              "\n"
              "a: Option Int = oo.map((x): x + 1)\n"
              "b: Option Int = oo.and_then((v): Some (v + 1))\n"
              "c: Option Int = oo.or_else((): Some 0)\n"
              "d: Option Int = oo.filter((v): v > 0)\n"
              "e: Bool = oo.is_some()\n"
              "f: Bool = oo.is_none()\n"
              "g: Int = oo.unwrap_or(0)\n"
              "h: Int = oo.unwrap()\n"
              "i: Int = oo.expect(\"value\")\n"
              "j: Option Int = Option.get(Some xs, 0)\n"
              "k: Result Int String = oo.to_result(\"none\")\n"
              "\n"
              "rr: Result Int String = Ok 1\n"
              "l: Result Int String = rr.try((v): Ok (v + 1))\n"
              "m: Result Int String = rr.map((v): v + 1)\n"
              "n: Result Int String = rr.map_err((e): e ++ \"!\")\n"
              "o: Result ([]Int) String = Result.traverse(xs, (v): Ok v)\n"
              "p: Bool = rr.is_ok()\n"
              "q: Bool = rr.is_err()\n"
              "r: Int = rr.unwrap_or(0)\n"
              "s: Int = rr.unwrap()\n"
              "t: Int = rr.expect(\"value\")\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

static void string_surface(void)
{
    cfx f;

    cfx_begin(&f,
              "s: String = \"a,b\"\n"
              "\n"
              "a: Option Char = s.at(1)\n"
              "b: []String = s.lines()\n"
              "c: []String = s.split(\",\")\n"
              "d: Bool = s.contains(\"a\")\n"
              "e: Bool = s.starts_with(\"a\")\n"
              "f: Bool = s.ends_with(\"b\")\n"
              "g: String = s.replace(\"a\", \"z\")\n"
              "h: String = s.replace_first(\"a\", \"z\")\n"
              "i: Option String = s.slice(0, 1)\n"
              "j: String = \"  x \".trim()\n"
              "k: String = \"  x \".trim_start()\n"
              "l: String = \"  x \".trim_end()\n"
              "m: Bytes = s.to_bytes()\n"
              "n: String = \"7\".pad_left(2, '0')\n"
              "o: String = \"7\".pad_right(2, '.')\n"
              "p: String = String.from_char('a')\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

static void time_surface(void)
{
    cfx f;

    cfx_begin(&f,
              "t: Time = Time 0\n"
              "m: Mono = Mono 0\n"
              "d: Duration = Duration 1\n"
              "\n"
              "a: Duration = (Time 1).sub(t)\n"
              "b: Time = t.add(Duration.secs(1))\n"
              "c: Int = t.to_ns()\n"
              "e: Duration = (Mono 1).sub(m)\n"
              "f: Mono = m.add(Duration.millis(2))\n"
              "g: Int = m.to_ns()\n"
              "h: Duration = d.add((Duration 2))\n"
              "i: Duration = Duration.secs(1).mul(2)\n"
              "j: Duration = d.neg()\n"
              "k: Int = Duration.millis(2).to_ns()\n"
              "l: Bool = True.not()\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

int main(void)
{
    list_surface();
    option_result_surface();
    string_surface();
    time_surface();
    return ek_test_report("check/std-surface");
}
