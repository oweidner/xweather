#ifndef MODEL_H
#define MODEL_H

#define FORECAST_DAYS 5

typedef struct {
    char   day_name[8];    /* e.g. "Sat" */
    char   date_label[16]; /* e.g. "Aug  1" */
    double high_c;         /* NAN if unavailable */
    double low_c;          /* NAN if unavailable */
    int    weather_code;   /* WMO weather code (-1 if unavailable) */
} DailyForecast;

#define HOURLY_SLOTS 10

typedef struct {
    char   hour_label[8]; /* "Now" for slot 0, else "HH:00"; empty if unavailable */
    double temperature_c; /* NAN if unavailable */
    int    weather_code;  /* WMO weather code (-1 if unavailable) */
    int    is_day;        /* 1 = daytime, 0 = nighttime (meaningless if weather_code < 0) */
} HourlySlot;

typedef struct WeatherModel WeatherModel;

typedef void (*WeatherObserverFn)(const char *location, const DailyForecast *days,
                                   const HourlySlot *hourly, double current_temperature_c,
                                   int current_is_day, double current_wind_speed_kmh,
                                   int current_precipitation_probability, void *client_data);

WeatherModel *weather_model_create(void);
void          weather_model_destroy(WeatherModel *model);

/* Fills `days` with FORECAST_DAYS placeholder entries meaning "no data yet":
 * NAN high/low, weather_code -1, and ASCII-only "N/A" labels (labels render
 * through a Latin-1-only core X font, so no UTF-8 here). */
void weather_forecast_fill_placeholder(DailyForecast days[FORECAST_DAYS]);

/* Fills `hourly` with HOURLY_SLOTS placeholder entries meaning "no data
 * yet": NAN temperature, weather_code -1, and an empty hour_label. */
void weather_hourly_fill_placeholder(HourlySlot hourly[HOURLY_SLOTS]);

void weather_model_set(WeatherModel *model, const char *location, const DailyForecast *days,
                        const HourlySlot *hourly, double current_temperature_c, int current_is_day,
                        double current_wind_speed_kmh, int current_precipitation_probability);

/* Registers fn to be called (with client_data) whenever the forecast changes. */
void weather_model_add_observer(WeatherModel *model, WeatherObserverFn fn, void *client_data);

#endif /* MODEL_H */
