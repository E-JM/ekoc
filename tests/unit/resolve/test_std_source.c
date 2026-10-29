#include "common.h"

#include "resolve/std_source.h"
#include "resolve/time_source.h"

/* The embedded std source is the release file's bytes. Regeneration is
   a tool, never a build step, so a stale table is a test failure: the
   compiler must parse exactly lib/std.eko and nothing else. */

static void embedded_matches_file(void)
{
    ek_arena *arena = ek_arena_create();
    char *src = rfx_read_file("lib/std.eko");
    char *joined;

    EK_CHECK(src != NULL);
    EK_CHECK(arena != NULL);
    if (!src || !arena) {
        return;
    }
    joined = ek_std_source_join(arena);
    EK_CHECK(joined != NULL);
    EK_CHECK_U64(ek_std_source_len, strlen(src));
    EK_CHECK(memcmp(joined, src, ek_std_source_len) == 0);
    ek_arena_destroy(arena);
    free(src);
}

/* The table carries the module's own declarations, so it is the std
   module and not a fragment of it. */
static void embedded_is_the_module(void)
{
    ek_arena *arena = ek_arena_create();
    char *joined;

    EK_CHECK(arena != NULL);
    if (!arena) {
        return;
    }
    joined = ek_std_source_join(arena);
    EK_CHECK(joined != NULL);
    EK_CHECK(strstr(joined, "assert_ne: (a: t, b: t) -> _ =") != NULL);
    EK_CHECK(joined[ek_std_source_len] == '\0');
    ek_arena_destroy(arena);
}

/* The time companion's table mirrors lib/std/time.eko byte for byte,
   like the std table above. */
static void embedded_time_matches_file(void)
{
    ek_arena *arena = ek_arena_create();
    char *src = rfx_read_file("lib/std/time.eko");
    char *joined;

    EK_CHECK(src != NULL);
    EK_CHECK(arena != NULL);
    if (!src || !arena) {
        return;
    }
    joined = ek_time_source_join(arena);
    EK_CHECK(joined != NULL);
    EK_CHECK_U64(ek_time_source_len, strlen(src));
    EK_CHECK(memcmp(joined, src, ek_time_source_len) == 0);
    EK_CHECK(joined[ek_time_source_len] == '\0');
    ek_arena_destroy(arena);
    free(src);
}

int main(void)
{
    embedded_matches_file();
    embedded_is_the_module();
    embedded_time_matches_file();
    return ek_test_report("resolve/std_source");
}
