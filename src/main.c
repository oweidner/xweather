#include <locale.h>

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

    config = config_load();
    locations = location_list_create();

    if (config_location_count(config) > 0) {
        int i;

        for (i = 0; i < config_location_count(config); i++)
            location_list_add(locations, config_location_name(config, i),
                               config_location_query(config, i));
    } else {
        /* No config file (or no "location" entries in it) -- fall back to a
         * small built-in default list. Display name stays ASCII (our labels
         * can't render arbitrary Unicode), but the geocoding query needs the
         * proper Turkish spelling to resolve at all: "Candarli" alone
         * returns zero results, "\xC3\x87andarl\xC4\xB1" (Çandarlı, UTF-8)
         * correctly finds the İzmir Province town. */
        location_list_add(locations, "Aachen", "Aachen");
        location_list_add(locations, "Apricale", "Apricale");
        location_list_add(locations, "Flensburg", "Flensburg");
        location_list_add(locations, "Candarli", "\xC3\x87" "andarl" "\xC4\xB1");
    }

    config_destroy(config);

    model = weather_model_create();
    view  = view_create(toplevel, locations);
    controller_create(app, model, locations, view);

    XtRealizeWidget(toplevel);

    controller_select_location(model, locations, 0);
    controller_prefetch_all();

    XtAppMainLoop(app);

    return 0;
}
