#include "common.h"

#include "resolve/std_source.h"
#include "resolve/time_source.h"
#include "resolve/regex_source.h"
#include "resolve/cap_source.h"
#include "resolve/sys_source.h"

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

/* One companion's table mirrors its release file byte for byte, like
   the std table above. */
static void embedded_companion(const char *file, char *(*join)(ek_arena *),
                               size_t len)
{
    ek_arena *arena = ek_arena_create();
    char *src = rfx_read_file(file);
    char *joined;

    EK_CHECK(src != NULL);
    EK_CHECK(arena != NULL);
    if (src && arena) {
        joined = join(arena);
        EK_CHECK(joined != NULL);
        if (joined) {
            EK_CHECK_U64(len, strlen(src));
            EK_CHECK(memcmp(joined, src, len) == 0);
            EK_CHECK(joined[len] == '\0');
        }
    }
    ek_arena_destroy(arena);
    free(src);
}

static void embedded_regex_matches_file(void)
{
    embedded_companion("lib/std/regex.eko", ek_regex_source_join,
                       ek_regex_source_len);
}

static void embedded_cap_matches_file(void)
{
    embedded_companion("cap/cap.eko", ek_cap_source_join,
                       ek_cap_source_len);
}

static void embedded_sys_matches_file(void)
{
    embedded_companion("cap/sys.eko", ek_sys_source_join,
                       ek_sys_source_len);
}

int main(void)
{
    embedded_matches_file();
    embedded_is_the_module();
    embedded_time_matches_file();
    embedded_regex_matches_file();
    embedded_cap_matches_file();
    embedded_sys_matches_file();
    return ek_test_report("resolve/std_source");
}
