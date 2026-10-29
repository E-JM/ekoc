#define _POSIX_C_SOURCE 200809L

#include "ekort.h"

#include <stddef.h>
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

void ek_panic(const char *code, const char *msg)
{
    write_all("panic [");
    write_all(code);
    write_all("]: ");
    write_all(msg);
    write_all("\n");
    exit(1);
}

void ek_abort(const char *reason)
{
    write_all("abort: ");
    write_all(reason);
    write_all("\n");
    abort();
}
