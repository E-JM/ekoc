#include "internal.h"

#include <pthread.h>

#include "conc.h"

/* The concurrency surface: a scope is a cancellation tree over tasks,
   a task is a detached thread running one closure, and cancellation is
   cooperative at world-call boundaries. The calling thread's task
   rides thread-local storage; a world call checks the pending flag
   before it executes and ends the task there, so a cancelling scope
   withholds the call it stands on and nothing after it. A pure
   computation is never interrupted, so a task that never reaches a
   boundary runs to its own end.

   No unwinding exists: a cancelled task abandons its frames and the
   references they hold, under the leak discipline of Clause 9.12.3. */

struct ek_scope {
    ek_obj obj;
    pthread_mutex_t lock;
    ek_task *children; /* one owned reference per child */
    int closed;
};

/* The value shape over the published layout: a handle value carries no
   field the unit reads, so the runtime's private typeinfo is the whole
   representation. */
typedef struct {
    ek_obj obj;
} ek_bare;

static void task_drop(ek_obj *obj);
static void scope_drop(ek_obj *obj);

static const ek_typeinfo task_info = {
    EK_KIND_STRUCT, task_drop, NULL, NULL
};
static const ek_typeinfo scope_info = {
    EK_KIND_STRUCT, scope_drop, NULL, NULL
};
static const ek_typeinfo conc_info = { EK_KIND_STRUCT, NULL, NULL, NULL };

static int is_task(ek_val v)
{
    return v.tag == EK_VAL_REF && v.as.ref != NULL
        && v.as.ref->info == &task_info;
}

static int is_scope(ek_val v)
{
    return v.tag == EK_VAL_REF && v.as.ref != NULL
        && v.as.ref->info == &scope_info;
}

/* The thread's own bookkeeping: runs on a normal return and on the
   pthread_exit a cancelled task performs, so the state is published
   either way and the thread's reference dies with the thread. */
static void task_thread_end(void *p)
{
    ek_task *t = p;

    pthread_mutex_lock(&t->lock);
    if (t->state == TASK_RUNNING) {
        t->state = TASK_CANCELLED;
    }
    pthread_cond_broadcast(&t->settled);
    pthread_mutex_unlock(&t->lock);
    ek_current_task = NULL;
    ek_release(ek_val_ref(&t->obj));
}

static void *task_main(void *p)
{
    ek_task *t = p;
    int cancel;

    pthread_cleanup_push(task_thread_end, t);
    ek_current_task = t;
    pthread_mutex_lock(&t->lock);
    cancel = t->cancel;
    pthread_mutex_unlock(&t->lock);
    if (!cancel) {
        /* The closure returned, so the task completed: cancellation is
           applied at world calls only, never as an after-the-fact
           verdict on a pure computation. */
        t->result = ek_closure_call(t->closure, NULL, 0);
        pthread_mutex_lock(&t->lock);
        t->state = TASK_DONE;
        pthread_mutex_unlock(&t->lock);
    }
    pthread_cleanup_pop(1);
    return NULL;
}

ek_val ek_cap_conc_os(void)
{
    ek_bare *c = ek_rt_alloc_zeroed(sizeof *c);

    c->obj.info = &conc_info;
    c->obj.rc = 1;
    return ek_val_ref(&c->obj);
}

/* Waits until the task has settled. The caller holds a reference to
   the task, so the wait cannot outlive the record. */
static int task_wait(ek_task *t)
{
    int state;

    pthread_mutex_lock(&t->lock);
    while (t->state == TASK_RUNNING) {
        pthread_cond_wait(&t->settled, &t->lock);
    }
    state = t->state;
    pthread_mutex_unlock(&t->lock);
    return state;
}

/* Scope exit: every child still running is cancelled at its next
   boundary, and exit waits for each. A child spinning in pure code
   never settles, so it hangs the exit rather than being interrupted. */
static void scope_cancel(ek_scope *s)
{
    ek_task *t;

    pthread_mutex_lock(&s->lock);
    s->closed = 1;
    for (t = s->children; t != NULL; t = t->next) {
        pthread_mutex_lock(&t->lock);
        if (t->state == TASK_RUNNING) {
            t->cancel = 1;
        }
        pthread_mutex_unlock(&t->lock);
    }
    pthread_mutex_unlock(&s->lock);
}

ek_val ek_conc_scope(ek_val conc, ek_val cb)
{
    ek_scope *s = ek_rt_alloc_zeroed(sizeof *s);
    ek_val sv;
    ek_val r;
    ek_task *t, *next;

    (void)conc;
    s->obj.info = &scope_info;
    s->obj.rc = 2; /* the callback's value and the scope call itself */
    pthread_mutex_init(&s->lock, NULL);
    sv = ek_val_ref(&s->obj);
    r = ek_closure_call(cb, &sv, 1);
    scope_cancel(s);
    for (t = s->children; t != NULL; t = next) {
        next = t->next;
        (void)task_wait(t);
        ek_release(ek_val_ref(&t->obj)); /* the scope's child reference */
    }
    ek_release(sv);
    ek_release(ek_val_ref(&s->obj));
    return r;
}

ek_val ek_scope_spawn(ek_val scope, ek_val cb)
{
    ek_scope *s;
    ek_task *t;
    pthread_t th;
    int closed;

    if (!is_scope(scope)) {
        return ek_val_unit();
    }
    s = (ek_scope *)scope.as.ref;
    t = ek_rt_alloc_zeroed(sizeof *t);
    t->obj.info = &task_info;
    t->obj.rc = 2; /* the returned value and the thread */
    pthread_mutex_init(&t->lock, NULL);
    pthread_cond_init(&t->settled, NULL);
    t->closure = cb;
    ek_retain(cb);
    t->scope = s;
    pthread_mutex_lock(&s->lock);
    closed = s->closed;
    if (!closed) {
        ek_retain(ek_val_ref(&t->obj)); /* the scope's child reference */
        t->next = s->children;
        s->children = t;
    }
    pthread_mutex_unlock(&s->lock);
    if (closed) {
        pthread_mutex_lock(&t->lock);
        t->cancel = 1;
        pthread_mutex_unlock(&t->lock);
    }
    if (pthread_create(&th, NULL, task_main, t) != 0) {
        ek_panic("P-panic", "Conc: a task thread cannot be created");
    }
    pthread_detach(th);
    return ek_val_ref(&t->obj);
}

ek_val ek_task_join(ek_val task)
{
    ek_task *t;
    int state;

    if (!is_task(task)) {
        return ek_gen_option_none();
    }
    t = (ek_task *)task.as.ref;
    state = task_wait(t);
    if (state == TASK_DONE) {
        return ek_gen_option_some(t->result);
    }
    return ek_gen_option_none();
}

ek_val ek_task_cancel(ek_val task)
{
    ek_task *t;

    if (is_task(task)) {
        t = (ek_task *)task.as.ref;
        pthread_mutex_lock(&t->lock);
        if (t->state == TASK_RUNNING) {
            t->cancel = 1;
        }
        pthread_mutex_unlock(&t->lock);
    }
    return ek_val_unit();
}

/* The record releases the closure and the result it owns; the thread's
   own reference is already gone when the drop can run. */
static void task_drop(ek_obj *obj)
{
    ek_task *t = (ek_task *)obj;

    ek_release(t->closure);
    ek_release(t->result);
    pthread_cond_destroy(&t->settled);
    pthread_mutex_destroy(&t->lock);
}

static void scope_drop(ek_obj *obj)
{
    ek_scope *s = (ek_scope *)obj;

    pthread_mutex_destroy(&s->lock);
}
