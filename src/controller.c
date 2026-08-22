#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <Xm/Xm.h>
#include <Xm/Protocols.h>
#include <Xm/ToggleB.h>

#include "config.h"
#include "controller.h"
#include "fetch_manager.h"
#include "weather_client.h"

/* Refetches every location's weather data on this interval, so cached data
 * (and, for the selected location, the "Updated ..." status text) doesn't
 * go stale while the app just sits open. */
#define REFRESH_INTERVAL_MS (30UL * 60UL * 1000UL)

/* Redraws the "Updated ..." status text on this interval (without fetching
 * anything), so it keeps ticking up between actual refreshes instead of
 * sitting frozen at whatever it said right after the last fetch. */
#define STATUS_TICK_INTERVAL_MS (30UL * 1000UL)

/* Bundles what the location-item callback (and the fetch-completion
 * callback) needs, since XtAddCallback/FetchCompletionFn only carry a
 * single client_data pointer. There is only ever one controller instance
 * in this app (same as model/locations/view being process-wide singletons
 * in main.c), so a single static pointer to it is kept below rather than
 * threading it through controller_select_location()'s existing signature. */
typedef struct {
    XtAppContext  app;
    WeatherModel *model;
    LocationList *locations;
    AppView      *view;
    FetchManager *fetch_mgr;
    int           selected_index; /* -1 until the first controller_select_location() */
    int           selected_has_error; /* set while the selected location's last fetch
                                        * attempt failed, so on_status_tick() knows to
                                        * leave the "Unable to fetch ..." message alone */
} LocationCallbackContext;

static LocationCallbackContext *g_ctx = NULL;

static void
on_weather_changed(const char *location, const DailyForecast *days, const HourlySlot *hourly,
                    double current_temperature_c, int current_is_day, double current_wind_speed_kmh,
                    int current_precipitation_probability, void *client_data)
{
    AppView *view = (AppView *)client_data;

    view_set_forecast(view, location, days, hourly, current_temperature_c, current_is_day,
                       current_wind_speed_kmh, current_precipitation_probability);
}

static void
on_quit(Widget w, XtPointer client_data, XtPointer call_data)
{
    (void)w;
    (void)client_data;
    (void)call_data;
    exit(EXIT_SUCCESS);
}

static void
on_about(Widget w, XtPointer client_data, XtPointer call_data)
{
    (void)w;
    (void)call_data;
    view_show_about_dialog((AppView *)client_data);
}

static void
on_manage_locations(Widget w, XtPointer client_data, XtPointer call_data)
{
    LocationCallbackContext *ctx = (LocationCallbackContext *)client_data;

    (void)w;
    (void)call_data;
    view_show_manage_locations_window(ctx->view, ctx->locations);
}

static void
on_location_selected(Widget w, XtPointer client_data, XtPointer call_data)
{
    LocationCallbackContext      *ctx = (LocationCallbackContext *)client_data;
    XmToggleButtonCallbackStruct *cbs = (XmToggleButtonCallbackStruct *)call_data;
    int                           i;

    if (!cbs->set)
        return;

    for (i = 0; i < ctx->view->num_location_items; i++) {
        if (ctx->view->location_items[i] == w) {
            controller_select_location(ctx->model, ctx->locations, i);
            return;
        }
    }
}

static void
on_view_selected(Widget w, XtPointer client_data, XtPointer call_data)
{
    AppView                      *view = (AppView *)client_data;
    XmToggleButtonCallbackStruct *cbs = (XmToggleButtonCallbackStruct *)call_data;

    if (!cbs->set)
        return;

    if (w == view->five_day_forecast_item) {
        view_show_daily_forecast(view);
        config_save_active_view("daily");
    } else if (w == view->hourly_forecast_item) {
        view_show_hourly_forecast(view);
        config_save_active_view("hourly");
    }
}

static void
set_error_state(WeatherModel *model)
{
    DailyForecast error_days[FORECAST_DAYS];
    HourlySlot    error_hourly[HOURLY_SLOTS];
    int           i;

    memset(error_days, 0, sizeof(error_days));
    for (i = 0; i < FORECAST_DAYS; i++) {
        error_days[i].high_c       = NAN;
        error_days[i].low_c        = NAN;
        error_days[i].weather_code = -1;
    }
    snprintf(error_days[0].date_label, sizeof(error_days[0].date_label), "No data");

    weather_hourly_fill_placeholder(error_hourly);

    weather_model_set(model, "Unable to fetch weather data", error_days, error_hourly, NAN, 1, NAN, -1);
}

/* Formats `when` relative to now (e.g. "just now", "5 minutes ago",
 * "2 hours ago") into buf. */
static void
format_relative_time(time_t when, char *buf, size_t buf_size)
{
    long diff_s = (long)difftime(time(NULL), when);
    long minutes, hours;

    if (diff_s < 60) {
        snprintf(buf, buf_size, "just now");
        return;
    }

    minutes = diff_s / 60;
    if (minutes < 60) {
        snprintf(buf, buf_size, "%ld minute%s ago", minutes, minutes == 1 ? "" : "s");
        return;
    }

    hours = minutes / 60;
    snprintf(buf, buf_size, "%ld hour%s ago", hours, hours == 1 ? "" : "s");
}

/* Refreshes the status bar for whichever location is currently on screen.
 * `fetch_failed` is only meaningful for the currently selected location --
 * background fetches of other locations never touch the status bar, same
 * as they never touch the model (see on_fetch_complete()). */
static void
update_status(AppView *view, const Location *loc, int fetch_failed)
{
    if (fetch_failed) {
        char status[192];

        snprintf(status, sizeof(status),
                 "Unable to fetch weather data for %s -- retrying automatically", loc->name);
        view_set_status(view, STATUS_AREA_LEFT, "");
        view_set_status(view, STATUS_AREA_MIDDLE, status);
        view_set_status(view, STATUS_AREA_RIGHT, "");
    } else if (loc->has_data) {
        char relative[32];
        char updated[64];

        format_relative_time(loc->last_updated, relative, sizeof(relative));
        snprintf(updated, sizeof(updated), "Updated %s", relative);

        view_set_status(view, STATUS_AREA_LEFT, "Data from open-meteo.com");
        view_set_status(view, STATUS_AREA_MIDDLE, "");
        view_set_status(view, STATUS_AREA_RIGHT, updated);
    } else {
        char status[128];

        snprintf(status, sizeof(status), "Fetching weather data for %s...", loc->name);
        view_set_status(view, STATUS_AREA_LEFT, "");
        view_set_status(view, STATUS_AREA_MIDDLE, status);
        view_set_status(view, STATUS_AREA_RIGHT, "");
    }
}

/* Runs on the Xt main thread (invoked from fetch_manager's XtAppAddInput
 * callback). Caches the result in `locations` regardless of whether it's
 * currently on screen; only pushes it into the model (and hence the view)
 * if this is the location the user currently has selected. */
static void
on_fetch_complete(int index, int success, const WeatherResult *result, void *client_data)
{
    LocationCallbackContext *ctx = (LocationCallbackContext *)client_data;

    if (success)
        location_list_set_data(ctx->locations, index, result->days, result->hourly,
                                result->current_temperature_c, result->current_is_day,
                                result->current_wind_speed_kmh, result->current_precipitation_probability);

    if (index != ctx->selected_index)
        return;

    if (success) {
        const Location *loc = location_list_get(ctx->locations, index);

        weather_model_set(ctx->model, loc->name, loc->days, loc->hourly, loc->current_temperature_c,
                           loc->current_is_day, loc->current_wind_speed_kmh,
                           loc->current_precipitation_probability);
        update_status(ctx->view, loc, 0);
        ctx->selected_has_error = 0;
    } else {
        set_error_state(ctx->model);
        update_status(ctx->view, location_list_get(ctx->locations, index), 1);
        ctx->selected_has_error = 1;
    }
}

/* Fires every STATUS_TICK_INTERVAL_MS and reschedules itself. Just redraws
 * the "Updated ..." text for whichever location is on screen from its
 * already-cached last_updated -- no fetch involved. Skipped while the
 * selected location's last fetch attempt failed, so it doesn't paper over
 * the "Unable to fetch ..." message with a stale timestamp. */
static void
on_status_tick(XtPointer client_data, XtIntervalId *id)
{
    LocationCallbackContext *ctx = (LocationCallbackContext *)client_data;

    (void)id;

    if (!ctx->selected_has_error && ctx->selected_index >= 0) {
        const Location *loc = location_list_get(ctx->locations, ctx->selected_index);

        if (loc->has_data)
            update_status(ctx->view, loc, 0);
    }

    XtAppAddTimeOut(ctx->app, STATUS_TICK_INTERVAL_MS, on_status_tick, ctx);
}

/* Fires every REFRESH_INTERVAL_MS: re-fetches every location (regardless of
 * whether it already has cached data) and reschedules itself. XtTimerCallback
 * only fires once per registration, so re-arming here is what makes this
 * periodic. */
static void
on_refresh_timeout(XtPointer client_data, XtIntervalId *id)
{
    LocationCallbackContext *ctx = (LocationCallbackContext *)client_data;
    int                      i, n = location_list_count(ctx->locations);

    (void)id;

    for (i = 0; i < n; i++) {
        const Location *loc = location_list_get(ctx->locations, i);

        fetch_manager_start(ctx->fetch_mgr, i, loc->query);
    }

    XtAppAddTimeOut(ctx->app, REFRESH_INTERVAL_MS, on_refresh_timeout, ctx);
}

/* Wires XmNvalueChangedCallback onto every current view->location_items[]
 * entry. Shared by controller_create() (startup) and
 * on_manage_locations_apply() (after a rebuild), since the widgets are
 * destroyed and recreated by view_rebuild_location_menu() each time. */
static void
wire_location_items(AppView *view)
{
    int i;

    for (i = 0; i < view->num_location_items; i++)
        XtAddCallback(view->location_items[i], XmNvalueChangedCallback, on_location_selected, g_ctx);
}

/* Fired by the manage-locations window's Apply button with the staged
 * (name, query) list. Rebuilds the app's location state from scratch --
 * fresh LocationList, fresh FetchManager, reselect index 0, refetch
 * everything -- exactly like a cold startup (see main.c), rather than
 * trying to incrementally patch the running LocationList/FetchManager
 * (which identify in-flight fetches purely by array index with no
 * cancellation, so an incremental edit could misattribute a stale fetch to
 * the wrong location after add/remove). The old LocationList and
 * FetchManager are intentionally leaked, not freed: nothing else in this
 * codebase frees them either (they only ever lived until process exit),
 * and freeing the old FetchManager out from under its detached worker
 * threads would be a use-after-free -- fetch_manager_stop() just stops
 * listening to it. */
static void
on_manage_locations_apply(const ConfigLocation *entries, int count, void *user_data)
{
    LocationCallbackContext *ctx = (LocationCallbackContext *)user_data;
    LocationList             *new_locations = location_list_create();
    int                       i;

    config_save_locations(entries, count);

    for (i = 0; i < count; i++)
        location_list_add(new_locations, entries[i].name, entries[i].query);

    fetch_manager_stop(ctx->fetch_mgr);
    ctx->fetch_mgr = fetch_manager_create(ctx->app, on_fetch_complete, ctx);
    ctx->locations = new_locations;

    view_rebuild_location_menu(ctx->view, ctx->locations);
    wire_location_items(ctx->view);

    controller_select_location(ctx->model, ctx->locations, 0);
    controller_prefetch_all();
}

void
controller_create(XtAppContext app, WeatherModel *model, LocationList *locations, AppView *view)
{
    Atom wm_delete_window;

    g_ctx = malloc(sizeof(*g_ctx));
    g_ctx->app                = app;
    g_ctx->model              = model;
    g_ctx->locations          = locations;
    g_ctx->view               = view;
    g_ctx->selected_index     = -1;
    g_ctx->selected_has_error = 0;
    g_ctx->fetch_mgr          = fetch_manager_create(app, on_fetch_complete, g_ctx);

    weather_model_add_observer(model, on_weather_changed, view);

    XtAddCallback(view->quit_item, XmNactivateCallback, on_quit, NULL);
    XtAddCallback(view->about_item, XmNactivateCallback, on_about, view);
    XtAddCallback(view->manage_locations_item, XmNactivateCallback, on_manage_locations, g_ctx);
    XtAddCallback(view->five_day_forecast_item, XmNvalueChangedCallback, on_view_selected, view);
    XtAddCallback(view->hourly_forecast_item, XmNvalueChangedCallback, on_view_selected, view);

    wire_location_items(view);
    location_manager_view_set_apply_callback(view->location_manager_view, on_manage_locations_apply, g_ctx);

    /* Make the window manager's close button behave the same as File -> Quit. */
    wm_delete_window = XmInternAtom(XtDisplay(view->toplevel), "WM_DELETE_WINDOW", False);
    XmAddWMProtocols(view->toplevel, &wm_delete_window, 1);
    XmAddWMProtocolCallback(view->toplevel, wm_delete_window, on_quit, NULL);

    XtAppAddTimeOut(app, REFRESH_INTERVAL_MS, on_refresh_timeout, g_ctx);
    XtAppAddTimeOut(app, STATUS_TICK_INTERVAL_MS, on_status_tick, g_ctx);
}

void
controller_select_location(WeatherModel *model, LocationList *locations, int index)
{
    const Location *loc = location_list_get(locations, index);

    g_ctx->selected_index     = index;
    g_ctx->selected_has_error = 0;

    /* Remembered across restarts (see main.c) -- whichever way selection
     * happened (startup, a menu click, or a Manage Locations Apply), the
     * newly active location is always the one that gets reselected next
     * launch. */
    config_save_active_location(loc->name);

    /* Never blocks: shows cached data if we have it, otherwise the N/A
     * placeholder location_list_add() filled in. Always kicks off a fresh
     * background fetch too (fetch_manager_start() no-ops if one's already
     * in flight), so switching to a location always picks up current data
     * rather than whatever was cached at startup or last refresh.
     * on_fetch_complete() updates the model again once that finishes. */
    weather_model_set(model, loc->name, loc->days, loc->hourly, loc->current_temperature_c,
                       loc->current_is_day, loc->current_wind_speed_kmh,
                       loc->current_precipitation_probability);
    update_status(g_ctx->view, loc, 0);

    fetch_manager_start(g_ctx->fetch_mgr, index, loc->query);
}

void
controller_prefetch_all(void)
{
    int i, n = location_list_count(g_ctx->locations);

    for (i = 0; i < n; i++) {
        const Location *loc = location_list_get(g_ctx->locations, i);

        if (!loc->has_data)
            fetch_manager_start(g_ctx->fetch_mgr, i, loc->query);
    }
}
