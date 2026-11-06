#include "internal.h"

#include <pthread.h>

#include "conc.h"

/* The cancellation point of the concurrency surface. A world call made
   inside a task reads the pending flag before the call: a task standing
   on a boundary with cancellation pending ends there, and that call is
   never made. Outside a task the call is the ordinary closure call, so
   every program carries the check without carrying the scheduler. */

__thread ek_task *ek_current_task;

ek_val ek_world_call(ek_val closure, ek_val *args, size_t argc)
{
    ek_task *t = ek_current_task;

    if (t != NULL) {
        int cancel;

        pthread_mutex_lock(&t->lock);
        cancel = t->cancel && t->state == TASK_RUNNING;
        if (cancel) {
            t->state = TASK_CANCELLED;
        }
        pthread_mutex_unlock(&t->lock);
        if (cancel) {
            pthread_exit(NULL);
        }
    }
    return ek_closure_call(closure, args, argc);
}
