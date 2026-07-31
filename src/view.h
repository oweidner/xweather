#ifndef VIEW_H
#define VIEW_H

#include <X11/Intrinsic.h>

#include "daily_forecast_view.h"
#include "hourly_forecast_view.h"
#include "locations.h"
#include "model.h"

/* Owns every widget the application creates. The controller wires
 * callbacks onto the widgets exposed here; the view never talks to
 * the model directly. */
typedef struct {
    Widget               toplevel;
    Widget               main_window;
    DailyForecastView   *daily_forecast_view;  /* "5-Day Forecast" view in the middle pane */
    HourlyForecastView  *hourly_forecast_view; /* "Hourly Forecast" view in the middle pane */
    Widget               current_icon;        /* current-conditions icon in the top pane */
    Widget               current_location;    /* current-conditions location name in the top pane */
    Widget               current_temperature; /* current-conditions temperature in the top pane */
    Widget               status_left;   /* left-aligned third of the status bar */
    Widget               status_middle; /* center-aligned third of the status bar */
    Widget               status_right;  /* right-aligned third of the status bar */
    Widget               quit_item;
    Widget               hourly_forecast_item; /* "Hourly Forecast" entry in the View menu */
    Widget               five_day_forecast_item; /* "5-Day Forecast" entry in the View menu */
    Widget               about_item;
    Widget               about_dialog; /* created lazily, reused across "Help -> About" clicks */
    Widget               manage_locations_item; /* "Manage..." entry in the Location menu */
    Widget               location_items[MAX_LOCATIONS]; /* one per LocationList entry, same order */
    int                  num_location_items;
} AppView;

/* Builds the menu bar (including a "Location" menu populated from
 * `locations`), the 3-pane layout, and both forecast views (5-Day Forecast
 * shown by default). */
AppView *view_create(Widget toplevel, const LocationList *locations);
void     view_destroy(AppView *view);

/* current_temperature_c is NAN to indicate no reading is available. */
void view_set_forecast(AppView *view, const char *location, const DailyForecast *days,
                        double current_temperature_c);
void view_set_window_title(AppView *view, const char *location, double current_temperature_c);
void view_show_about_dialog(AppView *view);

/* Switches the middle pane between the two forecast views, unmanaging the
 * one not shown. */
void view_show_daily_forecast(AppView *view);
void view_show_hourly_forecast(AppView *view);

typedef enum {
    STATUS_AREA_LEFT,
    STATUS_AREA_MIDDLE,
    STATUS_AREA_RIGHT
} StatusArea;

/* Sets the plain-text message shown in one third of the status bar fixed to
 * the bottom of the main window (e.g. STATUS_AREA_LEFT for the data-source
 * attribution, STATUS_AREA_RIGHT for "Last updated ..."). Each area updates
 * independently of the other two. ASCII only -- the status bar areas are
 * XmLabels and inherit the same Latin-1-only core X font as the rest of the
 * app's in-window labels. */
void view_set_status(AppView *view, StatusArea area, const char *text);

#endif /* VIEW_H */
