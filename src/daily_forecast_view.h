#ifndef DAILY_FORECAST_VIEW_H
#define DAILY_FORECAST_VIEW_H

#include <X11/Intrinsic.h>

#include "model.h"

typedef struct DailyForecastView DailyForecastView;

/* Builds a FORECAST_DAYS-column forecast panel as a managed child of
 * `parent`, filling it (attached on all four sides). Each column is a
 * DailyDataTile (see daily_data_tile.h), which owns its own card styling,
 * background, and icon selection. */
DailyForecastView *daily_forecast_view_create(Widget parent);
void                daily_forecast_view_destroy(DailyForecastView *view);

void daily_forecast_view_set_forecast(DailyForecastView *view, const DailyForecast *days);

/* The view's own top-level widget, so the caller can XtManageChild/
 * XtUnmanageChild it to switch between forecast views sharing one pane. */
Widget daily_forecast_view_widget(DailyForecastView *view);

#endif /* DAILY_FORECAST_VIEW_H */
