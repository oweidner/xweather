#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <X11/Intrinsic.h> /* XtAppContext */

#include "locations.h"
#include "model.h"
#include "view.h"

/* Registers itself as a model observer and wires callbacks onto the
 * view's widgets, so it never needs to be looked up again. Also creates the
 * background fetch manager, so `app` must be the same context the caller
 * will later run XtAppMainLoop() on. */
void controller_create(XtAppContext app, WeatherModel *model, LocationList *locations, AppView *view);

/* Makes locations[index] the active/displayed location: shows whatever
 * data `locations[index]` currently holds (cached, or the N/A placeholder)
 * immediately, without blocking. If that location hasn't been fetched yet,
 * starts (or lets an already-running) background fetch for it -- the model
 * is updated again asynchronously once that fetch completes. */
void controller_select_location(WeatherModel *model, LocationList *locations, int index);

/* Starts a background fetch for every location that doesn't have data yet
 * and isn't already being fetched. Call once at startup, after
 * controller_create() and XtRealizeWidget(). */
void controller_prefetch_all(void);

#endif /* CONTROLLER_H */
