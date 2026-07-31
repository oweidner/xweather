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

typedef struct WeatherModel WeatherModel;

typedef void (*WeatherObserverFn)(const char *location, const DailyForecast *days,
                                   double current_temperature_c, void *client_data);

WeatherModel *weather_model_create(void);
void          weather_model_destroy(WeatherModel *model);

/* Fills `days` with FORECAST_DAYS placeholder entries meaning "no data yet":
 * NAN high/low, weather_code -1, and ASCII-only "N/A" labels (labels render
 * through a Latin-1-only core X font, so no UTF-8 here). */
void weather_forecast_fill_placeholder(DailyForecast days[FORECAST_DAYS]);

void weather_model_set(WeatherModel *model, const char *location, const DailyForecast *days,
                        double current_temperature_c);

/* Registers fn to be called (with client_data) whenever the forecast changes. */
void weather_model_add_observer(WeatherModel *model, WeatherObserverFn fn, void *client_data);

#endif /* MODEL_H */
