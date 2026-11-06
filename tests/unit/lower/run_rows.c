#define _POSIX_C_SOURCE 200809L

#include "common.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* The executable gate for the new wiring: the value slots, the flat
   tuple destructuring, the set vocabulary, the radix channels, the
   dictionary traversals, the Json round trip over the stamped
   identities, and the store round trip that ties them together. The
   rig mirrors the one in run.c. */

static int write_unit(const char *path, const char *text, size_t len)
{
    FILE *fp = fopen(path, "wb");

    if (!fp) {
        return 0;
    }
    if (fwrite(text, 1, len, fp) != len) {
        fclose(fp);
        return 0;
    }
    return fclose(fp) == 0;
}

static int compile_unit(const char *name)
{
    char cmd[256];
    int n;

    n = snprintf(cmd, sizeof cmd,
                 "cc -std=c99 -O2 -pthread -Irt build/%s.c build/libekort.a"
                 " -lm -o build/%s", name, name);
    if (n < 0 || (size_t)n >= sizeof cmd) {
        return 0;
    }
    return system(cmd) == 0;
}

/* Drains one pipe into the buffer, NUL-terminated and capped. */
static void read_pipe(int fd, char *buf, size_t cap)
{
    size_t len = 0;
    ssize_t got;

    while (len + 1 < cap) {
        got = read(fd, buf + len, cap - 1 - len);
        if (got <= 0) {
            break;
        }
        len += (size_t)got;
    }
    buf[len] = '\0';
}

/* Runs the binary in a child whose standard output and standard error
   are the two pipes; returns the wait status with the captured text. */
static int run_binary(const char *path, char *out, size_t outcap,
                      char *err, size_t errcap)
{
    int fds[2], efs[2];
    pid_t pid;
    int status = -1;

    out[0] = '\0';
    err[0] = '\0';
    if (pipe(fds) != 0 || pipe(efs) != 0) {
        return -1;
    }
    pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        close(efs[0]);
        close(efs[1]);
        return -1;
    }
    if (pid == 0) {
        close(fds[0]);
        close(efs[0]);
        if (dup2(fds[1], STDOUT_FILENO) < 0
            || dup2(efs[1], STDERR_FILENO) < 0) {
            _exit(127);
        }
        close(fds[1]);
        close(efs[1]);
        execl(path, path, (char *)NULL);
        _exit(127);
    }
    close(fds[1]);
    close(efs[1]);
    read_pipe(fds[0], out, outcap);
    close(fds[0]);
    read_pipe(efs[0], err, errcap);
    close(efs[0]);
    waitpid(pid, &status, 0);
    return status;
}

/* Lowers, writes, compiles, and runs one program of fns functions;
   returns the wait status with the captured output and error. */
static int gate(const char *src, const char *name, size_t fns, char *out,
                size_t outcap, char *err, size_t errcap)
{
    lfx f;
    char path[64];
    int status;

    lfx_begin(&f, src);
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(f.low.text != NULL);
    EK_CHECK_U64(f.low.fn_count, fns);
    snprintf(path, sizeof path, "build/%s.c", name);
    EK_CHECK(write_unit(path, f.low.text, f.low.len));
    lfx_end(&f);
    EK_CHECK(compile_unit(name));
    snprintf(path, sizeof path, "build/%s", name);
    status = run_binary(path, out, outcap, err, errcap);
    EK_CHECK(status >= 0 && WIFEXITED(status));
    return status;
}

/* A top-level record value and a string constant flow through main:
   the lazy slots initialize at first use and the tuple destructuring
   reads the components. */
static void values_through_main(void)
{
    static const char *src =
        "Task: type = [id: Int, title: String, done: Bool]\n"
        "Store: type = [next_id: Int, tasks: []Task]\n"
        "_path: String = \"todos.json\"\n"
        "empty: Store = {next_id: 1, tasks: {}}\n"
        "pair: [Int, String] = {7, \"x\"}\n"
        "main: (out: Out) ! =\n"
        "    (a, b) = pair\n"
        "    out.println(_path)\n"
        "    out.println(show(empty.next_id))\n"
        "    out.println(show(a))\n"
        "    out.println(b)\n";
    char out[128], err[8];
    int status = gate(src, "lower_rows_values", 5, out, sizeof out,
                      err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, "todos.json\n1\n7\nx\n");
    EK_CHECK(err[0] == '\0');
}

/* The set vocabulary: of deduplicates keeping the first occurrences in
   order, insert and remove are copy-on-insert, and the old set keeps
   its own elements. */
static void set_members_run(void)
{
    static const char *src =
        "main: (out: Out) ! =\n"
        "    s := Set.of({3, 1, 3, 2})\n"
        "    out.println(s.length().show())\n"
        "    out.println(show(s.contains(2)))\n"
        "    out.println(show(s.contains(9)))\n"
        "    out.println(s.to_list().join(\", \"))\n"
        "    s9 := s.insert(9)\n"
        "    out.println(s9.length().show())\n"
        "    out.println(show(s9.contains(3)))\n"
        "    sr := s9.remove(3)\n"
        "    out.println(show(sr.contains(3)))\n"
        "    out.println(s9.to_list().join(\", \"))\n";
    char out[128], err[8];
    int status = gate(src, "lower_rows_set", 6, out, sizeof out,
                      err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, "3\nTrue\nFalse\n3, 1, 2\n4\nTrue\nFalse\n"
                      "3, 1, 2, 9\n");
    EK_CHECK(err[0] == '\0');
}

/* The radix channel prints the shortest forms and the machine words
   pad on the left, never truncating. */
static void radix_and_pad_run(void)
{
    static const char *src =
        "main: (out: Out) ! =\n"
        "    n: Int = 255\n"
        "    z: Int = 0\n"
        "    o: Int = 10\n"
        "    b: Int = 5\n"
        "    w: I32 = 255\n"
        "    x: U64 = 1\n"
        "    out.println(n.to_hex())\n"
        "    out.println(o.to_oct())\n"
        "    out.println(b.to_bin())\n"
        "    out.println(z.to_hex())\n"
        "    out.println(w.to_hex_pad(4))\n"
        "    out.println(x.to_hex_pad(2))\n";
    char out[64], err[8];
    int status = gate(src, "lower_rows_radix", 2, out, sizeof out,
                      err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, "ff\n12\n101\n0\n00ff\n01\n");
    EK_CHECK(err[0] == '\0');
}

/* The dictionary traversals fold, map, and filter over the insertion
   order a literal builds. */
static void dict_traversal_run(void)
{
    static const char *src =
        "main: (out: Out) ! =\n"
        "    d: [Int]String = {2: \"b\", 1: \"a\", 3: \"c\"}\n"
        "    s: String = d.fold(\"\", (acc, k, v): acc ++ \"{k}={v},\")\n"
        "    out.println(s)\n"
        "    m: [Int]String = d.map((k, v): v ++ \"!\")\n"
        "    out.println(m.get(1).unwrap_or(\"?\"))\n"
        "    f: [Int]String = d.filter((k, v): k > 1)\n"
        "    out.println(f.length().show())\n";
    char out[64], err[8];
    int status = gate(src, "lower_rows_dict", 3, out, sizeof out,
                      err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, "2=b,1=a,3=c,\na!\n2\n");
    EK_CHECK(err[0] == '\0');
}

/* The parse row routes to the runtime reader, the Result arms match
   over the stamped identities, and Json.show over the parsed value
   prints the minimal text form in the text's own key order. */
static void json_round_trip_run(void)
{
    static const char *src =
        "main: (out: Out) ! =\n"
        "    case Json.parse(\"\\{\\\"a\\\": [1, 2.5, true, null], "
        "\\\"b\\\": \\\"s\\\"}\"):\n"
        "        Ok j -> out.println(Json.show(j))\n"
        "        Err _ -> out.println(\"no\")\n";
    char out[64], err[8];
    int status = gate(src, "lower_rows_json", 15, out, sizeof out,
                      err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, "{\"a\":[1,2.5,true,null],\"b\":\"s\"}\n");
    EK_CHECK(err[0] == '\0');
}

/* The store round trip: a top-level record value seeds the store, the
   set vocabulary pins the id uniqueness, the inverted chain carries a
   lambda whose free name reaches through the continuation's capture
   set, and the derived JSON pair round trips over the runtime's
   identities. */
static void store_round_trip_run(void)
{
    static const char *src =
        "Task: type = [id: Int, title: String, done: Bool]\n"
        "Store: type = [next_id: Int, tasks: []Task]\n"
        "seed: Store = {next_id: 1, tasks: {}}\n"
        "load: (tag: String, text: String) -> Result Store String =\n"
        "    j <- Json.parse(text).map_err((e): \"{tag}: parse {e}\")"
        ".try()\n"
        "    s <- Store.from_json(j).map_err((e): \"{tag}: decode {e}\")"
        ".try()\n"
        "    Ok s\n"
        "deduped: (s: Store) -> Bool =\n"
        "    ids := Set.of(s.tasks.map((t): t.id))\n"
        "    ids.length() == s.tasks.length()\n"
        "run: (a: Task) -> Result Store String =\n"
        "    one := seed.with({tasks: {a}})\n"
        "    s <- load(\"store\", Json.show(one.to_json())).try()\n"
        "    Ok s\n"
        "main: (out: Out) ! =\n"
        "    a: Task = {id: 1, title: \"write docs\", done: False}\n"
        "    dupe: Task = {id: 1, title: \"dupe\", done: False}\n"
        "    out.println(show(deduped(seed.with({tasks: {a, dupe}}))))\n"
        "    out.println(show(deduped(seed.with({tasks: {a}}))))\n"
        "    case run(a):\n"
        "        Ok s -> out.println(show(s.next_id))\n"
        "        Err e -> out.println(e)\n"
        "    case run(a):\n"
        "        Ok s -> out.println(Json.show(s.to_json()))\n"
        "        Err e -> out.println(e)\n";
    char out[128], err[8];
    int status = gate(src, "lower_rows_store", 29, out, sizeof out,
                      err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, "False\nTrue\n1\n{\"next_id\":1,\"tasks\":"
                      "[{\"id\":1,\"title\":\"write docs\","
                      "\"done\":false}]}\n");
    EK_CHECK(err[0] == '\0');
}

int main(void)
{
    if (mkdir("build", 0777) != 0 && errno != EEXIST) {
        fprintf(stderr, "lower/run_rows: cannot create build/\n");
        return 1;
    }
    values_through_main();
    set_members_run();
    radix_and_pad_run();
    dict_traversal_run();
    json_round_trip_run();
    store_round_trip_run();
    return ek_test_report("lower/run_rows");
}
