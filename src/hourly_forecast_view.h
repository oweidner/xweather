#ifndef HOURLY_FORECAST_VIEW_H
#define HOURLY_FORECAST_VIEW_H

#include <X11/Intrinsic.h>

#include "model.h"

typedef struct HourlyForecastView HourlyForecastView;

/* Builds an hourly-forecast panel as a managed child of `parent`, filling it
 * (attached on all four sides), styled with the same darkened background as
 * the daily forecast view's cards. Internally, the card holds HOURLY_SLOTS
 * independent columns, each an HourlyDataTile (see hourly_data_tile.h),
 * which owns its own layout, day/night background switching, and icon
 * selection. Content starts blank/default until
 * hourly_forecast_view_set_forecast() is called. */
HourlyForecastView *hourly_forecast_view_create(Widget parent);
void                 hourly_forecast_view_destroy(HourlyForecastView *view);

void hourly_forecast_view_set_forecast(HourlyForecastView *view, const HourlySlot *hourly);

/* The view's own top-level widget, so the caller can XtManageChild/
 * XtUnmanageChild it to switch between forecast views sharing one pane. */
Widget hourly_forecast_view_widget(HourlyForecastView *view);

#endif /* HOURLY_FORECAST_VIEW_H */
