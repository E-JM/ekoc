#include "common.h"
#include "refcheck.h"

#include <sys/stat.h>

/* The reference-table invariants of resolve.h: strict node-address
   order, and exactly one ref per reference site over whole module
   closures (the conformance set plus a fixture tree). */

#define CONF_DIR "tests/conformance"
#define MOD_DIR "build/tmp-resolve/refs"

static const char *modules[] = {
    "access_drilling", "bindings", "case_patterns", "data_services",
    "derived_services", "generics", "geolib", "geolib_use",
    "lambdas_capture", "result_chains", "selective_import",
    "types_composites"
};

static int refs_of_file(const char *path, const char *name,
                        const char *const *mpath, size_t npath)
{
    char *src = rfx_read_file(path);
    int bad;

    if (!src) {
        fprintf(stderr, "resolve/refs: cannot read %s\n", path);
        return 1;
    }
    {
        rfx f;

        rfx_run(&f, src, mpath, npath, name);
        bad = rfx_check_refs(&f);
        if (bad) {
            fprintf(stderr, "resolve/refs: %s: %d violations\n", name, bad);
        }
        rfx_end(&f);
    }
    free(src);
    return bad != 0;
}

static void conformance_closures(void)
{
    char path[512], name[512];
    const char *mpath[1] = { CONF_DIR };
    size_t i;

    for (i = 0; i < sizeof modules / sizeof *modules; i++) {
        snprintf(path, sizeof path, "%s/%s.eko", CONF_DIR, modules[i]);
        snprintf(name, sizeof name, "%s.eko", modules[i]);
        EK_CHECK_U64((unsigned long long)refs_of_file(path, name, mpath, 1),
                     0);
    }
}

static void put_fixture_tree(void)
{
    mkdir("build/tmp-resolve", 0755);
    mkdir(MOD_DIR, 0755);
    EK_CHECK(rfx_put(MOD_DIR, "shapes.eko",
        "Shape: type = Circle Float | Box [w: Float, h: Float]\n"
        "area: (s: Shape) -> Float =\n"
        "    case s:\n"
        "        Circle r -> r * 3.0\n"
        "        Box {w, h} -> w * h\n"
        "_pad: Float = 0.5\n"));
}

/* One file that touches every reference position at once: string
   slots, composite keys, lambdas, case patterns with rests, type
   variables, the std bridge, a private member, and a module-shaped
   member link. */
static void every_position(void)
{
    rfx f;
    const char *mpath[1] = { MOD_DIR };

    put_fixture_tree();
    rfx_run(&f,
        "import shapes\n"
        "\n"
        "label: (s: Shape, t: t) -> String = \"{s} of {t}\"\n"
        "\n"
        "twice: (g: (Int) -> Int, v: Int) -> Int = g(g(v))\n"
        "\n"
        "pick: (xs: []Shape) -> Float =\n"
        "    case xs:\n"
        "        {Circle r, ..rest} -> r\n"
        "        _ -> shapes._pad\n"
        "\n"
        "run: () -> _ =\n"
        "    m: [tag: String, v: Int] = {tag: \"a\", v: 1}\n"
        "    t: Int = twice((n): n * 2, 1)\n"
        "    s: String = Float.to_fixed(1.5, 2)\n"
        "    std.inspect(shapes.area(m))\n"
        "    m.geolib.nothing()\n"
        "    _\n", mpath, 1, "refs.eko");
    EK_CHECK_U64(rfx_check_refs(&f), 0);
    rfx_end(&f);
}

int main(void)
{
    conformance_closures();
    every_position();
    return ek_test_report("resolve/refs");
}
