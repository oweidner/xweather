#ifndef HOURLY_FORECAST_VIEW_H
#define HOURLY_FORECAST_VIEW_H

#include <X11/Intrinsic.h>

typedef struct HourlyForecastView HourlyForecastView;

/* Builds an hourly-forecast panel (currently empty -- no hourly data source
 * is wired up yet) as a managed child of `parent`, filling it (attached on
 * all four sides), styled with the same darkened background as the daily
 * forecast view's cards. */
HourlyForecastView *hourly_forecast_view_create(Widget parent);
void                 hourly_forecast_view_destroy(HourlyForecastView *view);

/* The view's own top-level widget, so the caller can XtManageChild/
 * XtUnmanageChild it to switch between forecast views sharing one pane. */
Widget hourly_forecast_view_widget(HourlyForecastView *view);

#endif /* HOURLY_FORECAST_VIEW_H */
