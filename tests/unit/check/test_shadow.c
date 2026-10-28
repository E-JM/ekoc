/* The receiver-form chain and the module segment: a record field of
   the module's spelling wins the link, and the chain continues as
   member links off the field's value; without the field, the module
   path stands and a miss reports against the module. */

#include "common.h"

#include <sys/stat.h>

#define SHADOW_DIR "build/tmp-check-shadow"

static int put_module(const char *name, const char *content)
{
    char path[512];
    FILE *fp;
    int ok;

    mkdir("build", 0755);
    mkdir(SHADOW_DIR, 0755);
    snprintf(path, sizeof path, "%s/%s", SHADOW_DIR, name);
    fp = fopen(path, "wb");
    if (!fp) {
        return 0;
    }
    ok = fwrite(content, 1, strlen(content), fp) == strlen(content);
    fclose(fp);
    return ok;
}

/* A field named like the module shadows the segment: the call reads
   the field's value and its namespace, not the module. */
static void field_shadows_module(void)
{
    static const char *const mpath[1] = {SHADOW_DIR};
    cfx f;

    EK_CHECK(put_module("shadow_util.eko",
        "Strukt: type = [name: String]\n"
        "Strukt.len: (s: Strukt) -> Int = 3\n"
        "len: (s: String) -> Int = 9\n"));
    cfx_run(&f,
        "import shadow_util\n"
        "\n"
        "Cell: type = [shadow_util: Strukt, tag: Int]\n"
        "\n"
        "c: Cell = {shadow_util: {name: \"x\"}, tag: 1}\n"
        "a: Int = c.shadow_util.len()\n", mpath, 1, "shadow_main.eko");
    EK_CHECK_U64(cfx_stage_errors(f.ds, f.dn), 0);
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 6, EK_RD_FIELD));
    EK_CHECK(cfx_reading_at(&f, 6, EK_RD_NAMESPACE));
    cfx_end(&f);
}

/* Without the field the module path stands, and a member the module
   does not declare reports against the module. */
static void module_path_stands(void)
{
    static const char *const mpath[1] = {SHADOW_DIR};
    cfx f;

    EK_CHECK(put_module("shadow_util.eko",
        "Strukt: type = [name: String]\n"
        "Strukt.len: (s: Strukt) -> Int = 3\n"));
    cfx_run(&f,
        "import shadow_util\n"
        "\n"
        "s: Strukt = {name: \"x\"}\n"
        "a: Int = s.shadow_util.len()\n", mpath, 1, "shadow_main.eko");
    EK_CHECK_U64(cfx_stage_errors(f.ds, f.dn), 0);
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_run(&f,
        "import shadow_util\n"
        "\n"
        "s: Strukt = {name: \"x\"}\n"
        "a: Int = s.shadow_util.nosuch()\n", mpath, 1, "shadow_main.eko");
    EK_CHECK_U64(cfx_stage_errors(f.ds, f.dn), 0);
    EK_CHECK(cfx_has_code(&f, "E-unbound-member"));
    cfx_end(&f);
}

/* The shadowed link that no station answers belongs to the field's
   value; the module behind the spelling is out of the chain. */
static void shadow_miss_belongs_to_field(void)
{
    static const char *const mpath[1] = {SHADOW_DIR};
    cfx f;

    EK_CHECK(put_module("shadow_util.eko",
        "Strukt: type = [name: String]\n"
        "len: (s: String) -> Int = 9\n"));
    cfx_run(&f,
        "import shadow_util\n"
        "\n"
        "Cell: type = [shadow_util: Strukt, tag: Int]\n"
        "\n"
        "c: Cell = {shadow_util: {name: \"x\"}, tag: 1}\n"
        "a: Int = c.shadow_util.len()\n", mpath, 1, "shadow_main.eko");
    EK_CHECK_U64(cfx_stage_errors(f.ds, f.dn), 0);
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 6));
    cfx_end(&f);
}

int main(void)
{
    field_shadows_module();
    module_path_stands();
    shadow_miss_belongs_to_field();
    return ek_test_report("check/shadow");
}
