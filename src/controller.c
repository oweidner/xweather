#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Xm/Xm.h>
#include <Xm/Protocols.h>
#include <Xm/ToggleB.h>

#include "controller.h"
#include "fetch_manager.h"
#include "weather_client.h"

/* Bundles what the location-item callback (and the fetch-completion
 * callback) needs, since XtAddCallback/FetchCompletionFn only carry a
 * single client_data pointer. There is only ever one controller instance
 * in this app (same as model/locations/view being process-wide singletons
 * in main.c), so a single static pointer to it is kept below rather than
 * threading it through controller_select_location()'s existing signature. */
typedef struct {
    WeatherModel *model;
    LocationList *locations;
    AppView      *view;
    FetchManager *fetch_mgr;
    int           selected_index; /* -1 until the first controller_select_location() */
} LocationCallbackContext;

static LocationCallbackContext *g_ctx = NULL;

static void
on_weather_changed(const char *location, const DailyForecast *days, double current_temperature_c,
                    void *client_data)
{
    AppView *view = (AppView *)client_data;

    view_set_forecast(view, location, days, current_temperature_c);
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
set_error_state(WeatherModel *model)
{
    DailyForecast error_days[FORECAST_DAYS];
    int           i;

    memset(error_days, 0, sizeof(error_days));
    for (i = 0; i < FORECAST_DAYS; i++) {
        error_days[i].high_c       = NAN;
        error_days[i].low_c        = NAN;
        error_days[i].weather_code = -1;
    }
    snprintf(error_days[0].date_label, sizeof(error_days[0].date_label), "No data");

    weather_model_set(model, "Unable to fetch weather data", error_days, NAN);
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
        location_list_set_data(ctx->locations, index, result->days, result->current_temperature_c);

    if (index != ctx->selected_index)
        return;

    if (success) {
        const Location *loc = location_list_get(ctx->locations, index);

        weather_model_set(ctx->model, loc->name, loc->days, loc->current_temperature_c);
    } else {
        set_error_state(ctx->model);
    }
}

void
controller_create(XtAppContext app, WeatherModel *model, LocationList *locations, AppView *view)
{
    Atom wm_delete_window;
    int  i;

    g_ctx = malloc(sizeof(*g_ctx));
    g_ctx->model          = model;
    g_ctx->locations      = locations;
    g_ctx->view           = view;
    g_ctx->selected_index = -1;
    g_ctx->fetch_mgr      = fetch_manager_create(app, on_fetch_complete, g_ctx);

    weather_model_add_observer(model, on_weather_changed, view);

    XtAddCallback(view->quit_item, XmNactivateCallback, on_quit, NULL);
    XtAddCallback(view->about_item, XmNactivateCallback, on_about, view);

    for (i = 0; i < view->num_location_items; i++)
        XtAddCallback(view->location_items[i], XmNvalueChangedCallback, on_location_selected, g_ctx);

    /* Make the window manager's close button behave the same as File -> Quit. */
    wm_delete_window = XmInternAtom(XtDisplay(view->toplevel), "WM_DELETE_WINDOW", False);
    XmAddWMProtocols(view->toplevel, &wm_delete_window, 1);
    XmAddWMProtocolCallback(view->toplevel, wm_delete_window, on_quit, NULL);
}

void
controller_select_location(WeatherModel *model, LocationList *locations, int index)
{
    const Location *loc = location_list_get(locations, index);

    g_ctx->selected_index = index;

    /* Never blocks: shows cached data if we have it, otherwise the N/A
     * placeholder location_list_add() filled in. on_fetch_complete() updates
     * the model again once a background fetch finishes. */
    weather_model_set(model, loc->name, loc->days, loc->current_temperature_c);

    if (!loc->has_data)
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
