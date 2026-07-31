#include <stdlib.h>

#include <X11/Xlib.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>

#include "hourly_forecast_view.h"

struct HourlyForecastView {
    Widget form;
};

/* Returns `context`'s current background darkened by `factor` (e.g. 0.85
 * for 15% darker), matching the daily forecast view's card background.
 * Identical helper to daily_forecast_view.c's darker_background() -- kept
 * as a small, deliberate duplicate rather than a shared module, same as
 * this codebase's existing BOLD_FONT/create_bold_render_table duplication
 * between view.c and daily_forecast_view.c. */
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

    view->form = XtVaCreateManagedWidget("hourlyForecastView", xmFormWidgetClass, parent,
                                          XmNtopAttachment, XmATTACH_FORM,
                                          XmNbottomAttachment, XmATTACH_FORM,
                                          XmNleftAttachment, XmATTACH_FORM,
                                          XmNrightAttachment, XmATTACH_FORM,
                                          NULL);

    background = darker_background(view->form, 0.85);
    XmChangeColor(view->form, background);

    return view;
}

void
hourly_forecast_view_destroy(HourlyForecastView *view)
{
    free(view);
}

Widget
hourly_forecast_view_widget(HourlyForecastView *view)
{
    return view->form;
}
