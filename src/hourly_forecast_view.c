#include <math.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>

#include "hourly_data_tile.h"
#include "hourly_forecast_view.h"

struct HourlyForecastView {
    Widget           form;
    Widget           box; /* the single card */
    HourlyDataTile  *tiles[HOURLY_SLOTS];
};

/* Returns `context`'s current background darkened by `factor` (e.g. 0.85
 * for 15% darker). Identical helper to daily_data_tile.c's/
 * hourly_data_tile.c's darker_background() -- kept as a small, deliberate
 * duplicate rather than a shared module, same as this codebase's existing
 * BOLD_FONT/create_bold_render_table duplication between view.c and
 * daily_data_tile.c. */
static Pixel
darker_background(Widget context, double factor)
{
    Display *dpy = XtDisplay(context);
    Colormap cmap;
    Pixel    bg;
    XColor   color;

    XtVaGetValues(context, XmNbackground, &bg, XmNcolormap, &cmap, NULL);

    color.pixel = bg;
    XQueryColor(dpy, cmap, &color);

    color.red   = (unsigned short)(color.red   * factor);
    color.green = (unsigned short)(color.green * factor);
    color.blue  = (unsigned short)(color.blue  * factor);

    return XAllocColor(dpy, cmap, &color) ? color.pixel : bg;
}

HourlyForecastView *
hourly_forecast_view_create(Widget parent)
{
    HourlyForecastView *view = calloc(1, sizeof(*view));
    Pixel                background;
    int                  i;

    view->form = XtVaCreateManagedWidget("hourlyForecastView", xmFormWidgetClass, parent,
                                          XmNtopAttachment, XmATTACH_FORM,
                                          XmNbottomAttachment, XmATTACH_FORM,
                                          XmNleftAttachment, XmATTACH_FORM,
                                          XmNrightAttachment, XmATTACH_FORM,
                                          NULL);

    /* Same card look as a daily_forecast_view day tile (etched-in shadow,
     * darkened background, 6px margins) -- just one, filling the pane
     * instead of five side by side. Each HourlyDataTile darkens this box's
     * own background again for its own shading, same as a day tile does
     * relative to daily_forecast_view's plain form. */
    background = darker_background(view->form, 0.85);

    view->box = XtVaCreateManagedWidget("hourlyBox", xmFormWidgetClass, view->form,
                                         XmNbackground, background,
                                         XmNshadowThickness, 2,
                                         XmNshadowType, XmSHADOW_ETCHED_IN,
                                         XmNmarginWidth, 6,
                                         XmNmarginHeight, 6,
                                         XmNtopAttachment, XmATTACH_FORM,
                                         XmNbottomAttachment, XmATTACH_FORM,
                                         XmNleftAttachment, XmATTACH_FORM,
                                         XmNleftOffset, 4,
                                         XmNrightAttachment, XmATTACH_FORM,
                                         XmNrightOffset, 4,
                                         NULL);
    XmChangeColor(view->box, background);

    for (i = 0; i < HOURLY_SLOTS; i++)
        view->tiles[i] = hourly_data_tile_create(view->box, i, HOURLY_SLOTS);

    return view;
}

void
hourly_forecast_view_destroy(HourlyForecastView *view)
{
    int i;

    for (i = 0; i < HOURLY_SLOTS; i++)
        hourly_data_tile_destroy(view->tiles[i]);
    free(view);
}

Widget
hourly_forecast_view_widget(HourlyForecastView *view)
{
    return view->form;
}

/* Bars are scaled against a fixed-width window centered on the series'
 * median, rather than the series' own [min, max] -- mapping straight to the
 * observed range makes a 2-3 degree hour-to-hour change look like a
 * near-empty-to-full swing. Centering a wider window on the median keeps
 * the scale adapted to the actual weather -- a hot day and a cold day don't
 * fight over one shared absolute range -- while damping small changes to a
 * proportionate size. The median (rather than the mean) is used as the
 * center so a single outlier hour doesn't shift the whole scale. */
#define BAR_SCALE_HALF_SPAN 10.0 /* degrees C the scale extends above/below the median */

static int
compare_double(const void *a, const void *b)
{
    double da = *(const double *)a, db = *(const double *)b;

    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

void
hourly_forecast_view_set_forecast(HourlyForecastView *view, const HourlySlot *hourly)
{
    double temps[HOURLY_SLOTS];
    int    count = 0;
    double median = 0.0;
    double scale_min, scale_max;
    int    i;

    for (i = 0; i < HOURLY_SLOTS; i++) {
        double t = hourly[i].temperature_c;

        if (!isnan(t))
            temps[count++] = t;
    }

    if (count > 0) {
        qsort(temps, count, sizeof(double), compare_double);
        median = (count % 2) ? temps[count / 2]
                              : (temps[count / 2 - 1] + temps[count / 2]) / 2.0;
    }

    /* Every tile's bar is scaled against the same [scale_min, scale_max] so
     * bar heights are comparable across the whole row -- computed once here
     * rather than each tile guessing its own range in isolation. */
    scale_min = median - BAR_SCALE_HALF_SPAN;
    scale_max = median + BAR_SCALE_HALF_SPAN;

    for (i = 0; i < HOURLY_SLOTS; i++)
        hourly_data_tile_set_data(view->tiles[i], &hourly[i], scale_min, scale_max);
}
