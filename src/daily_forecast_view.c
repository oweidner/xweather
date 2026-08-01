#include <stdlib.h>

#include <Xm/Xm.h>
#include <Xm/Form.h>

#include "daily_data_tile.h"
#include "daily_forecast_view.h"

struct DailyForecastView {
    Widget          form;
    DailyDataTile  *tiles[FORECAST_DAYS];
};

DailyForecastView *
daily_forecast_view_create(Widget parent)
{
    DailyForecastView *view = calloc(1, sizeof(*view));
    int                 i;

    view->form = XtVaCreateManagedWidget("dailyForecastView", xmFormWidgetClass, parent,
                                          XmNtopAttachment, XmATTACH_FORM,
                                          XmNbottomAttachment, XmATTACH_FORM,
                                          XmNleftAttachment, XmATTACH_FORM,
                                          XmNrightAttachment, XmATTACH_FORM,
                                          NULL);

    for (i = 0; i < FORECAST_DAYS; i++)
        view->tiles[i] = daily_data_tile_create(view->form, i, FORECAST_DAYS);

    return view;
}

void
daily_forecast_view_destroy(DailyForecastView *view)
{
    int i;

    for (i = 0; i < FORECAST_DAYS; i++)
        daily_data_tile_destroy(view->tiles[i]);
    free(view);
}

Widget
daily_forecast_view_widget(DailyForecastView *view)
{
    return view->form;
}

void
daily_forecast_view_set_forecast(DailyForecastView *view, const DailyForecast *days)
{
    int i;

    for (i = 0; i < FORECAST_DAYS; i++)
        daily_data_tile_set_data(view->tiles[i], &days[i]);
}
