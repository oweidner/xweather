#ifndef LOCATION_MANAGER_VIEW_H
#define LOCATION_MANAGER_VIEW_H

#include <X11/Intrinsic.h>

#include "config.h"    /* ConfigLocation */
#include "locations.h" /* LocationList -- only to seed the editable list on show */

typedef struct LocationManagerView LocationManagerView;

/* Fired when "Apply" is clicked with a non-empty list. `entries`/`count`
 * are only valid for the duration of the call. */
typedef void (*LocationManagerApplyFn)(const ConfigLocation *entries, int count, void *user_data);

/* Builds the "Manage Locations" window as its own top-level shell (child of
 * `toplevel`, popped down until location_manager_view_show() is called), so
 * it gets its own window-manager frame/taskbar entry, independent of the
 * main window. Titled "XWeather - Manage Locations" and carries the same
 * window-manager icon as the main window. */
LocationManagerView *location_manager_view_create(Widget toplevel);
void                  location_manager_view_destroy(LocationManagerView *view);

/* Registers the callback fired on Apply. Call once, from the controller,
 * before the window is first shown. */
void location_manager_view_set_apply_callback(LocationManagerView *view,
                                               LocationManagerApplyFn on_apply, void *user_data);

/* Resets the window's editable list from `locations`'s current entries and
 * pops the window up. The search box, Add button, and each row's remove
 * button only edit this in-window copy -- nothing outside the window is
 * touched until Apply. Apply invokes the registered callback (unless the
 * list is empty) and closes the window, same as Cancel; Cancel (or the
 * window's own close button) just closes it, discarding any edits -- the
 * next location_manager_view_show() starts fresh from `locations` again. */
void location_manager_view_show(LocationManagerView *view, const LocationList *locations);

#endif /* LOCATION_MANAGER_VIEW_H */
