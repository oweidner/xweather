#ifndef WEATHER_WIDGET_H
#define WEATHER_WIDGET_H

#include <X11/Intrinsic.h>

#include "model.h"

typedef struct WeatherWidget WeatherWidget;

/* Builds a FORECAST_DAYS-column forecast panel as managed children of
 * `parent`, with vertical separators between columns. Each card's
 * background is a darker shade of `parent`'s own background, computed and
 * applied internally. */
WeatherWidget *weather_widget_create(Widget parent);
void           weather_widget_destroy(WeatherWidget *widget);

void weather_widget_set_forecast(WeatherWidget *widget, const DailyForecast *days);

#endif /* WEATHER_WIDGET_H */
