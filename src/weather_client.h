#ifndef WEATHER_CLIENT_H
#define WEATHER_CLIENT_H

#include <stddef.h> /* size_t */

#include "model.h"

typedef struct {
    char          location[128];
    DailyForecast days[FORECAST_DAYS];
    HourlySlot    hourly[HOURLY_SLOTS];
    double        current_temperature_c;          /* NAN if unavailable */
    int           current_is_day;                 /* 1 = daytime, 0 = nighttime */
    double        current_wind_speed_kmh;          /* NAN if unavailable */
    int           current_precipitation_probability; /* percent, -1 if unavailable */
} WeatherResult;

/* Geocodes `place` and fetches its FORECAST_DAYS-day forecast (including
 * today) plus the next HOURLY_SLOTS hours (starting with the current hour)
 * from Open-Meteo. Returns 0 and fills *result on success, -1 on any
 * network/parse failure. */
int weather_client_fetch(const char *place, WeatherResult *result);

#define MAX_GEOCODE_RESULTS 10

/* One candidate match from weather_client_geocode_search() -- just enough
 * to tell same-named places apart in a picker UI (e.g. "Paris" in France
 * vs. Texas). admin1/country are empty strings when Open-Meteo doesn't
 * provide them for a given result. */
typedef struct {
    char name[64];
    char admin1[64];  /* state/region */
    char country[64];
} GeocodeResult;

/* Searches Open-Meteo's geocoding API for `query`, filling up to
 * max_results candidate matches into `results`. Returns the number of
 * matches found (0 for none), or -1 on network/parse failure. */
int weather_client_geocode_search(const char *query, GeocodeResult *results, int max_results);

/* Formats `result` as "Name, Admin1, Country" (shorter when Open-Meteo
 * didn't provide admin1/country) -- the canonical way this app identifies a
 * location, both for display and as the query string to store, since it's
 * specific enough that re-geocoding it later reliably lands back on this
 * same candidate rather than some other place that happens to share its
 * bare name (e.g. "Paris" in Texas vs. France). */
void weather_client_geocode_format(const GeocodeResult *result, char *out, size_t out_size);

#endif /* WEATHER_CLIENT_H */
