#ifndef VIEW_H
#define VIEW_H

#include <X11/Intrinsic.h>

#include "locations.h"
#include "model.h"
#include "weather_widget.h"

/* Owns every widget the application creates. The controller wires
 * callbacks onto the widgets exposed here; the view never talks to
 * the model directly. */
typedef struct {
    Widget         toplevel;
    Widget         main_window;
    WeatherWidget *weather_widget;
    Widget         quit_item;
    Widget         about_item;
    Widget         about_dialog; /* created lazily, reused across "Help -> About" clicks */
    Widget         manage_locations_item; /* "Manage..." entry in the Location menu */
    Widget         location_items[MAX_LOCATIONS]; /* one per LocationList entry, same order */
    int            num_location_items;
} AppView;

/* Builds the menu bar (including a "Location" menu populated from
 * `locations`), the 3-pane layout, and the forecast widget. */
AppView *view_create(Widget toplevel, const LocationList *locations);
void     view_destroy(AppView *view);

/* current_temperature_c is NAN to indicate no reading is available. */
void view_set_forecast(AppView *view, const char *location, const DailyForecast *days,
                        double current_temperature_c);
void view_set_window_title(AppView *view, const char *location, double current_temperature_c);
void view_show_about_dialog(AppView *view);

#endif /* VIEW_H */
