#ifndef LOCATIONS_H
#define LOCATIONS_H

#include <time.h>

#include "model.h" /* DailyForecast, FORECAST_DAYS */

#define MAX_LOCATIONS 16

typedef struct {
    char          name[64];  /* displayed in the menu/title; kept ASCII since
                               * our labels can't render arbitrary Unicode */
    char          query[96]; /* sent to the geocoding API; may need accents
                               * or other characters `name` can't carry */
    DailyForecast days[FORECAST_DAYS];
    double        current_temperature_c; /* meaningless until has_data is set */
    int           has_data; /* 0 until weather data has been fetched for this location */
    time_t        last_updated; /* wall-clock time of the last successful fetch;
                                  * meaningless until has_data is set */
} Location;

typedef struct LocationList LocationList;

LocationList *location_list_create(void);
void          location_list_destroy(LocationList *list);

/* Registers a location (sorted into place by `name`), with no weather data
 * yet. `query` is what gets geocoded; pass `name` again if they're the same. */
void location_list_add(LocationList *list, const char *name, const char *query);

int              location_list_count(const LocationList *list);
const Location  *location_list_get(const LocationList *list, int index);

/* Stores fetched weather data for the entry at `index` and marks it as
 * having data (see Location.has_data). */
void location_list_set_data(LocationList *list, int index, const DailyForecast *days,
                             double current_temperature_c);

#endif /* LOCATIONS_H */
