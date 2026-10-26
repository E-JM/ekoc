#ifndef EK_SUPPORT_DIAG_H
#define EK_SUPPORT_DIAG_H

#include <stdarg.h>
#include <stddef.h>

typedef enum {
    EK_SEV_ERROR,
    EK_SEV_WARNING
} ek_severity;

/* Codes ("E-...", "W-...") are opaque strings here; NULL means
   the diagnostic carries no code. code and file are borrowed and must
   outlive the buffer; message is owned by it. */
typedef struct {
    ek_severity severity;
    const char *code;
    const char *message;
    const char *file;
    int line;
    int col;
} ek_diagnostic;

typedef struct ek_diag ek_diag;

/* Returns NULL when the buffer cannot be allocated. */
ek_diag *ek_diag_create(void);
void ek_diag_destroy(ek_diag *diag);

/* fmt is a printf-style format. Emission never aborts; a diagnostic the
   host cannot store is dropped. */
void ek_diag_emit(ek_diag *diag, ek_severity severity, const char *code,
                  const char *file, int line, int col,
                  const char *fmt, ...);
void ek_diag_emitv(ek_diag *diag, ek_severity severity, const char *code,
                   const char *file, int line, int col,
                   const char *fmt, va_list args);

size_t ek_diag_count(const ek_diag *diag, ek_severity severity);

/* Empties the buffer and returns the drained diagnostics in emission
   order; *count receives their number when count is not NULL. The
   storage is valid until the next emit or destroy. */
const ek_diagnostic *ek_diag_drain(ek_diag *diag, size_t *count);

#endif
