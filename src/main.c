#include <locale.h>
#include <string.h>

#include <curl/curl.h>
#include <Xm/Xm.h>

#include "model.h"
#include "config.h"
#include "locations.h"
#include "view.h"
#include "controller.h"

int
main(int argc, char **argv)
{
    XtAppContext  app;
    Widget        toplevel;
    WeatherModel *model;
    LocationList *locations;
    AppView      *view;
    AppConfig    *config;

    curl_global_init(CURL_GLOBAL_DEFAULT);

    /* Without this, Xt stays in the "C" locale and XmStringCreateLocalized
     * misreads our UTF-8 strings (e.g. the degree sign) one byte at a time. */
    XtSetLanguageProc(NULL, NULL, NULL);

    toplevel = XtVaAppInitialize(&app, "XWeather", NULL, 0,
                                  &argc, argv, NULL, NULL);

    /* XtVaAppInitialize re-invokes the language proc while opening the
     * display, re-applying LC_ALL from the environment (e.g. a comma
     * decimal separator). Pin LC_NUMERIC back to "C" here, after that
     * happens, so snprintf("%f", ...) keeps producing dot-decimal numbers
     * for the API URLs we build. */
    setlocale(LC_NUMERIC, "C");

    config = config_load_locations();
    locations = location_list_create();

    if (config_location_count(config) > 0) {
        int i;

        for (i = 0; i < config_location_count(config); i++)
            location_list_add(locations, config_location_name(config, i),
                               config_location_query(config, i));
    } else {
        /* No locations file (or no "location" entries in it) -- fall back to a
         * small built-in default list. Each is the full "City, Region,
         * Country" string Open-Meteo's geocoding search itself returns for
         * that place (the same convention config.c's own DEFAULT_LOCATION_NAMES
         * and the Manage Locations search use), so it resolves to exactly
         * one place rather than whatever the geocoder ranks first for a
         * bare, ambiguous name. */
        location_list_add(locations, "Aachen, North Rhine-Westphalia, Germany",
                           "Aachen, North Rhine-Westphalia, Germany");
        location_list_add(locations, "Apricale, Liguria, Italy",
                           "Apricale, Liguria, Italy");
        location_list_add(locations, "Flensburg, Schleswig-Holstein, Germany",
                           "Flensburg, Schleswig-Holstein, Germany");
    }

    config_destroy(config);

    model = weather_model_create();
    view  = view_create(toplevel, locations);
    controller_create(app, model, locations, view);

    XtRealizeWidget(toplevel);

    {
        char saved_active[96];
        int  start_index = 0;

        /* Reselect whichever location was active last time, if it's still
         * in the list -- otherwise fall back to the first (alphabetical)
         * entry, same as before this existed. */
        if (config_load_active_location(saved_active, sizeof(saved_active))) {
            int found = location_list_find(locations, saved_active);

            if (found >= 0)
                start_index = found;
        }

        controller_select_location(model, locations, start_index);

        /* controller_select_location() only updates the model/fetch state --
         * the Location menu's own radio checkmark still needs to be told
         * about a non-default startup selection explicitly (it's only ever
         * set at widget-creation time, for index 0, otherwise). */
        view_set_selected_location_item(view, start_index);
    }

    {
        char saved_view[16];

        /* Same idea for which forecast view was showing -- "5-Day Forecast"
         * is view_create()'s own built-in default, so only switch away from
         * it if the state file says otherwise. */
        if (config_load_active_view(saved_view, sizeof(saved_view)) &&
            strcmp(saved_view, "hourly") == 0)
            view_show_hourly_forecast(view);
    }

    controller_prefetch_all();

    XtAppMainLoop(app);

    return 0;
}
