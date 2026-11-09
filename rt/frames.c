#include "ekort.h"

#include <stddef.h>

/* The frame stack behind the panic backtrace. The generated code of
   every named function pushes its source-level name and declaration
   position on entry and pops it at the return, so a panic can print
   the live calls innermost first without touching the heap. The stack
   is bounded: beyond its depth the newest frames are dropped, and the
   backtrace shows the outer calls that fit.

   The same counter is the stack-overflow guard (Clause B.6): every
   generated call pushes, so a runaway recursion crosses the limit
   before the machine stack dies, and the abort line reports it. The
   limit sits at half the depth the default 8 MiB stack carries of the
   generated frames, the heaviest of which hold their own keep and
   release temporaries. */

#define EK_FRAME_LIMIT 20000

ek_frame ek_frames[EK_FRAME_MAX];
size_t ek_frame_depth;

void ek_frame_push(const char *name, const char *file, int line, int col)
{
    if (ek_frame_depth >= EK_FRAME_LIMIT) {
        ek_frame_depth = 0;
        ek_abort("stack overflow");
    }
    if (ek_frame_depth < EK_FRAME_MAX) {
        ek_frame *f = &ek_frames[ek_frame_depth];

        f->name = name;
        f->file = file;
        f->line = line;
        f->col = col;
    }
    ek_frame_depth++;
}

void ek_frame_pop(void)
{
    if (ek_frame_depth > 0) {
        ek_frame_depth--;
    }
}

/* The call site of the live call: the caller marks its own frame with
   the position of the call it is making, so a panic in the callee
   points at the line that asked for it. The generated code emits this
   before a call whose callee pushes a frame. */
void ek_frame_site(int line, int col)
{
    if (ek_frame_depth > 0 && ek_frame_depth <= EK_FRAME_MAX) {
        ek_frame *f = &ek_frames[ek_frame_depth - 1];

        f->line = line;
        f->col = col;
    }
}
