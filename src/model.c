#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "model.h"

#define MAX_OBSERVERS 8
#define MAX_LOCATION  128

typedef struct {
    WeatherObserverFn fn;
    void              *client_data;
} Observer;

struct WeatherModel {
    char          location[MAX_LOCATION];
    DailyForecast days[FORECAST_DAYS];
    HourlySlot    hourly[HOURLY_SLOTS];
    double        current_temperature_c;
    int           current_is_day;
    Observer      observers[MAX_OBSERVERS];
    int           num_observers;
};

void
weather_forecast_fill_placeholder(DailyForecast days[FORECAST_DAYS])
{
    int i;

    memset(days, 0, FORECAST_DAYS * sizeof(*days));
    for (i = 0; i < FORECAST_DAYS; i++) {
        strncpy(days[i].day_name, "N/A", sizeof(days[i].day_name) - 1);
        strncpy(days[i].date_label, "N/A", sizeof(days[i].date_label) - 1);
        days[i].high_c       = NAN;
        days[i].low_c        = NAN;
        days[i].weather_code = -1;
    }
}

void
weather_hourly_fill_placeholder(HourlySlot hourly[HOURLY_SLOTS])
{
    int i;

    memset(hourly, 0, HOURLY_SLOTS * sizeof(*hourly));
    for (i = 0; i < HOURLY_SLOTS; i++) {
        hourly[i].temperature_c = NAN;
        hourly[i].weather_code  = -1;
        hourly[i].is_day        = 1;
    }
}

WeatherModel *
weather_model_create(void)
{
    WeatherModel *model = calloc(1, sizeof(WeatherModel));

    weather_forecast_fill_placeholder(model->days);
    weather_hourly_fill_placeholder(model->hourly);
    model->current_temperature_c = NAN;
    model->current_is_day        = 1;

    return model;
}

void
weather_model_destroy(WeatherModel *model)
{
    free(model);
}

static void
weather_model_notify(WeatherModel *model)
{
    int i;

    for (i = 0; i < model->num_observers; i++) {
        model->observers[i].fn(model->location, model->days, model->hourly,
                                model->current_temperature_c, model->current_is_day,
                                model->observers[i].client_data);
    }
}

void
weather_model_set(WeatherModel *model, const char *location, const DailyForecast *days,
                   const HourlySlot *hourly, double current_temperature_c, int current_is_day)
{
    strncpy(model->location, location, MAX_LOCATION - 1);
    model->location[MAX_LOCATION - 1] = '\0';
    memcpy(model->days, days, sizeof(model->days));
    memcpy(model->hourly, hourly, sizeof(model->hourly));
    model->current_temperature_c = current_temperature_c;
    model->current_is_day        = current_is_day;
    weather_model_notify(model);
}

void
weather_model_add_observer(WeatherModel *model, WeatherObserverFn fn, void *client_data)
{
    if (model->num_observers >= MAX_OBSERVERS)
        return;

    model->observers[model->num_observers].fn          = fn;
    model->observers[model->num_observers].client_data = client_data;
    model->num_observers++;
}
