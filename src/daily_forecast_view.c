#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Label.h>

#include "daily_forecast_view.h"
#include "weather_icons.h"

/* Motif has no font-independent "bold" resource on XmLabel; weight is only
 * ever expressed as part of a font descriptor. This asks for bold at a
 * larger pixel size and wildcards everything else, so the X server still
 * matches it against the same default font family (here, "Misc Fixed"). */
#define BOLD_FONT "-*-*-bold-r-*-*-18-*-*-*-*-*-*-*"

typedef struct {
    Widget day_name;
    Widget date;
    Widget icon;
    Widget high;
    Widget low;
} DayColumn;

struct DailyForecastView {
    Widget    form;
    DayColumn days[FORECAST_DAYS];
};

/* Returns `context`'s current background darkened by `factor` (e.g. 0.85
 * for 15% darker), so the view's cards stand out from their parent. */
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

static XmRenderTable
create_bold_render_table(Widget context)
{
    Arg           args[2];
    XmRendition   rendition;
    XmRenderTable table;

    XtSetArg(args[0], XmNfontType, XmFONT_IS_FONT);
    XtSetArg(args[1], XmNfontName, BOLD_FONT);

    rendition = XmRenditionCreate(context, XmFONTLIST_DEFAULT_TAG, args, 2);
    table = XmRenderTableAddRenditions(NULL, &rendition, 1, XmMERGE_NEW);
    XmRenditionFree(rendition);

    return table;
}

/* Our labels render through plain core X fonts (ISO8859-1), which can't
 * display multi-byte UTF-8 text. XmStringCreate (unlike ...Localized) takes
 * the bytes as-is for the given charset tag, with no locale-driven
 * reinterpretation, so callers can hand it the raw Latin-1 byte for '°'. */
static void
set_label_text(Widget label, const char *text)
{
    XmString xmstr = XmStringCreate((char *)text, XmFONTLIST_DEFAULT_TAG);

    XtVaSetValues(label, XmNlabelString, xmstr, NULL);
    XmStringFree(xmstr);
}

static void
create_day_column(Widget parent, int index, Pixel bg, DayColumn *out)
{
    Widget column;

    column = XtVaCreateManagedWidget("dayColumn", xmFormWidgetClass, parent,
                                      XmNbackground, bg,
                                      XmNshadowThickness, 2,
                                      XmNshadowType, XmSHADOW_ETCHED_IN,
                                      XmNmarginWidth, 6,
                                      XmNmarginHeight, 6,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNbottomAttachment, XmATTACH_FORM,
                                      XmNleftAttachment, XmATTACH_POSITION,
                                      XmNleftPosition, index * 100 / FORECAST_DAYS,
                                      XmNleftOffset, 4,
                                      XmNrightAttachment, XmATTACH_POSITION,
                                      XmNrightPosition, (index + 1) * 100 / FORECAST_DAYS,
                                      XmNrightOffset, 4,
                                      NULL);
    XmChangeColor(column, bg);

    {
        XmRenderTable bold_table = create_bold_render_table(column);

        out->day_name = XtVaCreateManagedWidget("dayName", xmLabelWidgetClass, column,
                                                 XmNbackground, bg,
                                                 XmNrenderTable, bold_table,
                                                 XmNtopAttachment, XmATTACH_POSITION,
                                                 XmNtopPosition, 15,
                                                 XmNleftAttachment, XmATTACH_FORM,
                                                 XmNrightAttachment, XmATTACH_FORM,
                                                 XmNalignment, XmALIGNMENT_CENTER,
                                                 NULL);
        XmRenderTableFree(bold_table);
    }

    out->date = XtVaCreateManagedWidget("dayDate", xmLabelWidgetClass, column,
                                         XmNbackground, bg,
                                         XmNtopAttachment, XmATTACH_WIDGET,
                                         XmNtopWidget, out->day_name,
                                         XmNtopOffset, 4,
                                         XmNleftAttachment, XmATTACH_FORM,
                                         XmNrightAttachment, XmATTACH_FORM,
                                         XmNalignment, XmALIGNMENT_CENTER,
                                         NULL);

    out->icon = XtVaCreateManagedWidget("dayIcon", xmLabelWidgetClass, column,
                                         XmNbackground, bg,
                                         XmNlabelType, XmPIXMAP,
                                         XmNlabelPixmap, weather_icon_for_code(column, 0),
                                         XmNtopAttachment, XmATTACH_WIDGET,
                                         XmNtopWidget, out->date,
                                         XmNtopOffset, 10,
                                         XmNleftAttachment, XmATTACH_FORM,
                                         XmNrightAttachment, XmATTACH_FORM,
                                         XmNalignment, XmALIGNMENT_CENTER,
                                         NULL);

    {
        XmRenderTable bold_table = create_bold_render_table(column);

        out->high = XtVaCreateManagedWidget("dayHigh", xmLabelWidgetClass, column,
                                             XmNbackground, bg,
                                             XmNrenderTable, bold_table,
                                             XmNtopAttachment, XmATTACH_WIDGET,
                                             XmNtopWidget, out->icon,
                                             XmNtopOffset, 10,
                                             XmNleftAttachment, XmATTACH_FORM,
                                             XmNrightAttachment, XmATTACH_FORM,
                                             XmNalignment, XmALIGNMENT_CENTER,
                                             NULL);
        XmRenderTableFree(bold_table);
    }

    out->low = XtVaCreateManagedWidget("dayLow", xmLabelWidgetClass, column,
                                        XmNbackground, bg,
                                        XmNtopAttachment, XmATTACH_WIDGET,
                                        XmNtopWidget, out->high,
                                        XmNtopOffset, 8,
                                        XmNleftAttachment, XmATTACH_FORM,
                                        XmNrightAttachment, XmATTACH_FORM,
                                        XmNalignment, XmALIGNMENT_CENTER,
                                        NULL);
}

DailyForecastView *
daily_forecast_view_create(Widget parent)
{
    DailyForecastView *view = calloc(1, sizeof(*view));
    Pixel               background;
    int                  i;

    view->form = XtVaCreateManagedWidget("dailyForecastView", xmFormWidgetClass, parent,
                                          XmNtopAttachment, XmATTACH_FORM,
                                          XmNbottomAttachment, XmATTACH_FORM,
                                          XmNleftAttachment, XmATTACH_FORM,
                                          XmNrightAttachment, XmATTACH_FORM,
                                          NULL);

    background = darker_background(view->form, 0.85);

    for (i = 0; i < FORECAST_DAYS; i++)
        create_day_column(view->form, i, background, &view->days[i]);

    return view;
}

void
daily_forecast_view_destroy(DailyForecastView *view)
{
    free(view);
}

Widget
daily_forecast_view_widget(DailyForecastView *view)
{
    return view->form;
}

static void
set_temperature_label(Widget label, const char *prefix, double temperature_c)
{
    char buf[32];

    if (isnan(temperature_c))
        set_label_text(label, "");
    else {
        snprintf(buf, sizeof(buf), "%s%.0f" "\xB0" "C", prefix, temperature_c);
        set_label_text(label, buf);
    }
}

void
daily_forecast_view_set_forecast(DailyForecastView *view, const DailyForecast *days)
{
    int i;

    for (i = 0; i < FORECAST_DAYS; i++) {
        Widget icon = view->days[i].icon;

        set_label_text(view->days[i].day_name, days[i].day_name);
        set_label_text(view->days[i].date, days[i].date_label);
        set_temperature_label(view->days[i].high, "H: ", days[i].high_c);
        set_temperature_label(view->days[i].low, "L: ", days[i].low_c);

        if (days[i].weather_code < 0) {
            XtUnmanageChild(icon);
        } else {
            XtVaSetValues(icon, XmNlabelPixmap, weather_icon_for_code(icon, days[i].weather_code), NULL);
            XtManageChild(icon);

            /* XmLabel doesn't reliably repaint on its own when only
             * XmNlabelPixmap changes (the resource updates, but the old
             * image stays on screen) - force an expose so it redraws. */
            if (XtIsRealized(icon))
                XClearArea(XtDisplay(icon), XtWindow(icon), 0, 0, 0, 0, True);
        }
    }
}
