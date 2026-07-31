#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fetch_manager.h"
#include "locations.h" /* MAX_LOCATIONS */

#define FETCH_RETRY_BASE_MS   30000UL /* 30s */
#define FETCH_RETRY_MAX_SHIFT 4       /* 30,60,120,240,480(cap) seconds */

struct FetchManager {
    XtAppContext       app;
    FetchCompletionFn  on_complete;
    void              *user_data;

    int                pipe_fds[2]; /* [0] read (Xt-registered), [1] write (workers) */
    XtInputId          input_id;

    /* Everything below is shared with worker threads and protected by lock. */
    pthread_mutex_t    lock;
    int                in_flight[MAX_LOCATIONS];
    int                has_completed[MAX_LOCATIONS];
    int                completed_success[MAX_LOCATIONS];
    WeatherResult      completed_result[MAX_LOCATIONS];
    char               query[MAX_LOCATIONS][96]; /* remembered for retries */
    int                retry_attempt[MAX_LOCATIONS];
    XtIntervalId       retry_timer[MAX_LOCATIONS]; /* 0 == none pending */
};

typedef struct {
    FetchManager *mgr;
    int           index;
    char          query[96];
} FetchTask;

typedef struct {
    FetchManager *mgr;
    int           index;
} RetryClosure;

static void schedule_retry(FetchManager *mgr, int index);
static void retry_timeout_cb(XtPointer client_data, XtIntervalId *id);

/* Runs on a worker thread: network I/O + JSON parsing only. Never touches
 * Xt/Motif, LocationList, or WeatherModel directly. */
static void *
fetch_worker_main(void *arg)
{
    FetchTask    *task = (FetchTask *)arg;
    FetchManager *mgr  = task->mgr;
    WeatherResult result;
    int           rc;
    int           ok;
    char          byte = 1;
    ssize_t       written;

    rc = weather_client_fetch(task->query, &result);
    ok = (rc == 0);

    printf("fetch_manager: location %d (\"%s\") fetched, weather_client_fetch() returned %d\n",
           task->index, task->query, rc);
    fflush(stdout);

    pthread_mutex_lock(&mgr->lock);
    mgr->completed_success[task->index] = ok;
    if (ok)
        mgr->completed_result[task->index] = result;
    mgr->has_completed[task->index] = 1;
    pthread_mutex_unlock(&mgr->lock);

    do {
        written = write(mgr->pipe_fds[1], &byte, 1);
    } while (written < 0 && errno == EINTR);

    free(task);
    return NULL;
}

/* Schedules an automatic retry for `index` with exponential backoff
 * (30s, 60s, ... capped at 480s), unless one is already pending -- this is
 * the single choke point that prevents a location from ever accumulating
 * more than one pending retry timer. Must run on the Xt main thread. */
static void
schedule_retry(FetchManager *mgr, int index)
{
    RetryClosure *closure;
    unsigned long shift;
    unsigned long delay_ms;

    pthread_mutex_lock(&mgr->lock);
    if (mgr->retry_timer[index] != 0) {
        pthread_mutex_unlock(&mgr->lock);
        return;
    }

    shift = (unsigned long)mgr->retry_attempt[index];
    if (shift > FETCH_RETRY_MAX_SHIFT)
        shift = FETCH_RETRY_MAX_SHIFT;
    mgr->retry_attempt[index]++;
    delay_ms = FETCH_RETRY_BASE_MS << shift;

    closure = malloc(sizeof(*closure));
    closure->mgr   = mgr;
    closure->index = index;

    mgr->retry_timer[index] = XtAppAddTimeOut(mgr->app, delay_ms, retry_timeout_cb, closure);
    pthread_mutex_unlock(&mgr->lock);
}

/* Fires on the Xt main thread when a retry's backoff timer elapses. */
static void
retry_timeout_cb(XtPointer client_data, XtIntervalId *id)
{
    RetryClosure *closure = (RetryClosure *)client_data;
    FetchManager *mgr     = closure->mgr;
    int           index   = closure->index;
    char          query[96];

    (void)id;

    pthread_mutex_lock(&mgr->lock);
    mgr->retry_timer[index] = 0;
    strncpy(query, mgr->query[index], sizeof(query) - 1);
    query[sizeof(query) - 1] = '\0';
    pthread_mutex_unlock(&mgr->lock);

    free(closure);

    fetch_manager_start(mgr, index, query);
}

void
fetch_manager_start(FetchManager *mgr, int index, const char *query)
{
    FetchTask     *task;
    pthread_t      thread;
    pthread_attr_t attr;

    pthread_mutex_lock(&mgr->lock);
    if (mgr->in_flight[index]) {
        pthread_mutex_unlock(&mgr->lock);
        return;
    }
    mgr->in_flight[index] = 1;
    strncpy(mgr->query[index], query, sizeof(mgr->query[index]) - 1);
    mgr->query[index][sizeof(mgr->query[index]) - 1] = '\0';
    pthread_mutex_unlock(&mgr->lock);

    printf("fetch_manager: starting fetch for location %d (\"%s\")\n", index, query);
    fflush(stdout);

    task = malloc(sizeof(*task));
    task->mgr   = mgr;
    task->index = index;
    strncpy(task->query, query, sizeof(task->query) - 1);
    task->query[sizeof(task->query) - 1] = '\0';

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if (pthread_create(&thread, &attr, fetch_worker_main, task) != 0) {
        fprintf(stderr, "fetch_manager: pthread_create failed for index %d\n", index);
        free(task);
        pthread_mutex_lock(&mgr->lock);
        mgr->in_flight[index] = 0;
        pthread_mutex_unlock(&mgr->lock);
        schedule_retry(mgr, index);
    }

    pthread_attr_destroy(&attr);
}

/* The XtAppAddInput callback: runs on the Xt main thread whenever the pipe
 * has data. Drains it (a wakeup, not a message -- byte count is unrelated
 * to completion count) then scans every slot for completions, so it's
 * immune to multiple workers finishing between two dispatcher iterations. */
static void
fetch_manager_input_cb(XtPointer client_data, int *source, XtInputId *id)
{
    FetchManager *mgr = (FetchManager *)client_data;
    char          buf[64];
    int           i;

    (void)id;

    while (read(*source, buf, sizeof(buf)) > 0)
        ; /* drain; EAGAIN/EWOULDBLOCK just ends the loop (fd is O_NONBLOCK) */

    for (i = 0; i < MAX_LOCATIONS; i++) {
        int           has;
        int           success = 0;
        WeatherResult result;

        pthread_mutex_lock(&mgr->lock);
        has = mgr->has_completed[i];
        if (has) {
            mgr->has_completed[i] = 0;
            mgr->in_flight[i]     = 0;
            success = mgr->completed_success[i];
            if (success)
                result = mgr->completed_result[i];
        }
        pthread_mutex_unlock(&mgr->lock);

        if (!has)
            continue;

        if (success) {
            pthread_mutex_lock(&mgr->lock);
            mgr->retry_attempt[i] = 0;
            if (mgr->retry_timer[i] != 0) {
                XtRemoveTimeOut(mgr->retry_timer[i]);
                mgr->retry_timer[i] = 0;
            }
            pthread_mutex_unlock(&mgr->lock);
        } else {
            schedule_retry(mgr, i);
        }

        mgr->on_complete(i, success, success ? &result : NULL, mgr->user_data);
    }
}

FetchManager *
fetch_manager_create(XtAppContext app, FetchCompletionFn on_complete, void *user_data)
{
    FetchManager *mgr = calloc(1, sizeof(*mgr));
    int           flags;

    mgr->app         = app;
    mgr->on_complete = on_complete;
    mgr->user_data   = user_data;

    pipe(mgr->pipe_fds);

    flags = fcntl(mgr->pipe_fds[0], F_GETFL, 0);
    fcntl(mgr->pipe_fds[0], F_SETFL, flags | O_NONBLOCK);

    pthread_mutex_init(&mgr->lock, NULL);

    mgr->input_id = XtAppAddInput(app, mgr->pipe_fds[0],
                                   (XtPointer)(intptr_t)XtInputReadMask,
                                   fetch_manager_input_cb, mgr);

    return mgr;
}
