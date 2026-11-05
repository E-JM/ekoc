#include "ekort.h"

#include <stddef.h>

/* The frame stack behind the panic backtrace. The generated code of
   every named function pushes its source-level name and declaration
   position on entry and pops it at the return, so a panic can print
   the live calls innermost first without touching the heap. The stack
   is bounded: beyond its depth the newest frames are dropped, and the
   backtrace shows the outer calls that fit. */

ek_frame ek_frames[EK_FRAME_MAX];
size_t ek_frame_depth;

void ek_frame_push(const char *name, const char *file, int line, int col)
{
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
