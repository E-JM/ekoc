#define _POSIX_C_SOURCE 200809L

#include "ekort.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Panic and abort terminate the process without unwinding: no release
   pass runs and no finalization obligation remains. Output goes to
   standard error through write, so reporting stays alive on a heap
   that may already be broken. */

static void write_all(const char *s)
{
    size_t len = strlen(s);
    size_t done = 0;

    while (done < len) {
        ssize_t n = write(STDERR_FILENO, s + done, len - done);
        if (n <= 0)
            return; /* a failed report never blocks the termination */
        done += (size_t)n;
    }
}

/* One backtrace frame: "  at name (file:line:column)", the live calls
   innermost first. The frame text is fixed and allocation-free. */
static void write_frame(size_t i)
{
    char pos[96];

    write_all("  at ");
    write_all(ek_frames[i].name);
    write_all(" (");
    write_all(ek_frames[i].file);
    snprintf(pos, sizeof pos, ":%d:%d)\n", ek_frames[i].line,
             ek_frames[i].col);
    write_all(pos);
}

void ek_panic(const char *code, const char *msg)
{
    size_t i = ek_frame_depth;

    write_all("panic [");
    write_all(code);
    write_all("]: ");
    write_all(msg);
    write_all("\n");
    while (i-- > 0) {
        if (i < EK_FRAME_MAX) {
            write_frame(i);
        }
    }
    exit(1);
}

void ek_abort(const char *reason)
{
    write_all("abort: ");
    write_all(reason);
    write_all("\n");
    abort();
}
