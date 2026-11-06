#ifndef EK_RT_CONC_H
#define EK_RT_CONC_H

#include <pthread.h>

#include "ekort.h"

/* The scheduler record shared by the concurrency surface and the
   world-call cancellation point. Private to rt. */

typedef enum {
    TASK_RUNNING,
    TASK_DONE,
    TASK_CANCELLED
} ek_task_state;

typedef struct ek_scope ek_scope;

typedef struct ek_task {
    ek_obj obj;
    pthread_mutex_t lock;
    pthread_cond_t settled;
    ek_val closure; /* owned */
    ek_val result;  /* owned, DONE only */
    ek_scope *scope;
    struct ek_task *next;
    int state;
    int cancel;
} ek_task;

/* The task of the calling thread, NULL outside any task. */
extern __thread ek_task *ek_current_task;

#endif
