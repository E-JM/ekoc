#include "support/diag.h"

#include <stdlib.h>
#include <string.h>

#include "support/arena.h"
#include "support/buf.h"

#define DIAG_ITEMS_MIN ((size_t)16)

struct ek_diag {
    ek_arena *text; /* message storage, freed wholesale */
    ek_diagnostic *items;
    size_t count;
    size_t cap;
};

ek_diag *ek_diag_create(void)
{
    struct ek_diag *diag = malloc(sizeof *diag);
    if (diag == NULL) {
        return NULL;
    }
    diag->text = ek_arena_create();
    if (diag->text == NULL) {
        free(diag);
        return NULL;
    }
    diag->items = NULL;
    diag->count = 0;
    diag->cap = 0;
    return diag;
}

void ek_diag_destroy(ek_diag *diag)
{
    if (diag == NULL) {
        return;
    }
    ek_arena_destroy(diag->text);
    free(diag->items);
    free(diag);
}

static int diag_grow(struct ek_diag *diag)
{
    size_t cap = diag->cap != 0 ? diag->cap * 2 : DIAG_ITEMS_MIN;
    ek_diagnostic *items = realloc(diag->items, cap * sizeof *items);
    if (items == NULL) {
        return -1;
    }
    diag->items = items;
    diag->cap = cap;
    return 0;
}

static void diag_store(struct ek_diag *diag, ek_severity severity,
                       const char *code, const char *file, int line, int col,
                       const char *message)
{
    ek_diagnostic *item;
    char *copy;
    size_t len = strlen(message) + 1;

    if (diag->count == diag->cap && diag_grow(diag) != 0) {
        return;
    }
    copy = ek_arena_alloc(diag->text, len);
    if (copy == NULL) {
        return;
    }
    memcpy(copy, message, len);
    item = &diag->items[diag->count++];
    item->severity = severity;
    item->code = code;
    item->message = copy;
    item->file = file;
    item->line = line;
    item->col = col;
}

void ek_diag_emitv(ek_diag *diag, ek_severity severity, const char *code,
                   const char *file, int line, int col,
                   const char *fmt, va_list args)
{
    ek_buf buf;

    ek_buf_init(&buf);
    ek_buf_vprintf(&buf, fmt, args);
    diag_store(diag, severity, code, file, line, col,
               buf.data != NULL ? buf.data : "");
    ek_buf_destroy(&buf);
}

void ek_diag_emit(ek_diag *diag, ek_severity severity, const char *code,
                  const char *file, int line, int col,
                  const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    ek_diag_emitv(diag, severity, code, file, line, col, fmt, args);
    va_end(args);
}

size_t ek_diag_count(const ek_diag *diag, ek_severity severity)
{
    size_t i;
    size_t n = 0;

    for (i = 0; i < diag->count; i++) {
        if (diag->items[i].severity == severity) {
            n++;
        }
    }
    return n;
}

const ek_diagnostic *ek_diag_drain(ek_diag *diag, size_t *count)
{
    const ek_diagnostic *items = diag->items;

    if (count != NULL) {
        *count = diag->count;
    }
    diag->count = 0;
    return items;
}
