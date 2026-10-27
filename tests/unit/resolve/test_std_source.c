#include "common.h"

#include "resolve/std_source.h"

/* The embedded std source is the release file's bytes. Regeneration is
   a tool, never a build step, so a stale table is a test failure: the
   compiler must parse exactly lib/std.eko and nothing else. */

static void embedded_matches_file(void)
{
    char *src = rfx_read_file("lib/std.eko");

    EK_CHECK(src != NULL);
    if (!src) {
        return;
    }
    EK_CHECK_U64(ek_std_source_len, strlen(src));
    EK_CHECK(memcmp(ek_std_source, src, ek_std_source_len) == 0);
    free(src);
}

/* The table carries the module's own declarations, so it is the std
   module and not a fragment of it. */
static void embedded_is_the_module(void)
{
    const char *needle = "assert_ne: (a: t, b: t) -> _ =";

    EK_CHECK(strstr(ek_std_source, needle) != NULL);
    EK_CHECK(ek_std_source[ek_std_source_len] == '\0');
}

int main(void)
{
    embedded_matches_file();
    embedded_is_the_module();
    return ek_test_report("resolve/std_source");
}
