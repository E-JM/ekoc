#include "internal.h"

#include <stdlib.h>
#include <string.h>

/* The lexical path vocabulary over String: joining, normalizing, and
   the absolute test, with no file-system contact. The operations are
   text only; they never touch the working directory. */

static ek_obj *path_from(const char *a, size_t len)
{
    return ek_str_new(a, len);
}

/* The joined path: a separator goes between the parts, and an empty
   part contributes nothing. */
ek_obj *ek_path_join(ek_obj *a, ek_obj *b)
{
    const char *pa = ek_str_cstr(a);
    const char *pb = ek_str_cstr(b);
    size_t la = ek_str_len(a);
    size_t lb = ek_str_len(b);

    if (la == 0) {
        return path_from(pb, lb);
    }
    if (lb == 0) {
        return path_from(pa, la);
    }
    {
        int sep = pa[la - 1] != '/';
        size_t len = la + (size_t)sep + lb;
        char *out = ek_rt_alloc_zeroed(len + 1);

        memcpy(out, pa, la);
        if (sep) {
            out[la] = '/';
        }
        memcpy(out + la + (size_t)sep, pb, lb);
        return path_from(out, len);
    }
}

/* The normalized path: `.` segments drop, `..` pops the previous
   segment, and a leading separator survives. An empty result is the
   empty relative path of the empty input; otherwise the root is ".". */
ek_obj *ek_path_normalize(ek_obj *path)
{
    const char *p = ek_str_cstr(path);
    size_t len = ek_str_len(path);
    const char **segs = ek_rt_alloc_zeroed((len + 1) * sizeof *segs);
    size_t *lens = ek_rt_alloc_zeroed((len + 1) * sizeof *lens);
    size_t n = 0, i = 0, out = 0;
    char *buf;
    int absolute = len > 0 && p[0] == '/';

    while (i < len) {
        size_t start = i;

        while (i < len && p[i] != '/') {
            i++;
        }
        if (i == start) {
            i++;
            continue;
        }
        if (i - start == 1 && p[start] == '.') {
            continue;
        }
        if (i - start == 2 && p[start] == '.' && p[start + 1] == '.'
            && n > 0 && !(lens[n - 1] == 2
                          && strncmp(segs[n - 1], "..", 2) == 0)) {
            n--;
            continue;
        }
        segs[n] = p + start;
        lens[n] = i - start;
        n++;
    }
    buf = ek_rt_alloc_zeroed(len + 2);
    if (absolute) {
        buf[out++] = '/';
    }
    for (i = 0; i < n; i++) {
        if (i > 0) {
            buf[out++] = '/';
        }
        memcpy(buf + out, segs[i], lens[i]);
        out += lens[i];
    }
    if (out == 0) {
        buf[out++] = '.';
    }
    return path_from(buf, out);
}

/* The absolute test: the path begins with the separator. */
ek_val ek_path_is_absolute(ek_obj *path)
{
    return ek_bool(ek_str_len(path) > 0 && ek_str_cstr(path)[0] == '/');
}
