#ifndef FETCH_MANAGER_H
#define FETCH_MANAGER_H

#include <X11/Intrinsic.h>

#include "weather_client.h"

typedef struct FetchManager FetchManager;

/* Invoked on the Xt main thread only, once a background fetch for `index`
 * finishes. `result` is only valid/meaningful when `success` is nonzero. */
typedef void (*FetchCompletionFn)(int index, int success, const WeatherResult *result,
                                   void *user_data);

/* Must be called from the Xt main thread, after curl_global_init() and
 * before any fetch_manager_start() calls. */
FetchManager *fetch_manager_create(XtAppContext app, FetchCompletionFn on_complete,
                                    void *user_data);

/* Starts a background fetch of `query` for location `index`, unless one is
 * already in flight for that index (a no-op in that case) -- safe to call
 * redundantly, e.g. once from a startup prefetch-all and again from
 * location selection. On failure, retries are scheduled automatically with
 * backoff; the caller doesn't need to do anything else. Must be called
 * from the Xt main thread. */
void fetch_manager_start(FetchManager *mgr, int index, const char *query);

/* Stops listening for this manager's background completions -- used when
 * replacing it wholesale (e.g. after the location list is rebuilt from
 * scratch). Any worker thread still finishing writes into this now-
 * abandoned instance harmlessly, since nothing reads it anymore. Does not
 * free `mgr`: it has no matching destroy, since its detached worker
 * threads can't be safely joined or cancelled -- the safe move is to stop
 * listening and leak it, not free it out from under them. */
void fetch_manager_stop(FetchManager *mgr);

#endif /* FETCH_MANAGER_H */
