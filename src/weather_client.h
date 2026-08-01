#ifndef WEATHER_CLIENT_H
#define WEATHER_CLIENT_H

#include "model.h"

typedef struct {
    char          location[128];
    DailyForecast days[FORECAST_DAYS];
    HourlySlot    hourly[HOURLY_SLOTS];
    double        current_temperature_c; /* NAN if unavailable */
    int           current_is_day;        /* 1 = daytime, 0 = nighttime */
} WeatherResult;

/* Geocodes `place` and fetches its FORECAST_DAYS-day forecast (including
 * today) plus the next HOURLY_SLOTS hours (starting with the current hour)
 * from Open-Meteo. Returns 0 and fills *result on success, -1 on any
 * network/parse failure. */
int weather_client_fetch(const char *place, WeatherResult *result);

#endif /* WEATHER_CLIENT_H */
