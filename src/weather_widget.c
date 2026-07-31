#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/xpm.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Label.h>

#include "weather_widget.h"

/* Motif has no font-independent "bold" resource on XmLabel; weight is only
 * ever expressed as part of a font descriptor. This asks for bold at a
 * larger pixel size and wildcards everything else, so the X server still
 * matches it against the same default font family (here, "Misc Fixed"). */
#define BOLD_FONT "-*-*-bold-r-*-*-18-*-*-*-*-*-*-*"

typedef enum {
    ICON_CLEAR,
    ICON_FEW_CLOUDS,
    ICON_OVERCAST,
    ICON_FOG,
    ICON_SHOWERS,
    ICON_SNOW,
    ICON_STORM,
    ICON_COUNT
} IconKind;

/* Pre-converted from the GNOME Weather app's own "small" icon variant
 * (package gnome-weather, /usr/share/icons/hicolor/scalable/status/weather-*
 * -small.svg; GPL-2+, see /usr/share/doc/gnome-weather/copyright), flattened
 * onto the same background color create_day_column() darkens its cards to.
 * Assumes the app is run from the project root, matching how this project
 * is built. assets/icons/32x32 holds the same set at a smaller size, for
 * contexts that don't need the full 64x64 (currently unused by this file). */
static const char *icon_files[ICON_COUNT] = {
    "assets/icons/64x64/weather-clear.xpm",
    "assets/icons/64x64/weather-few-clouds.xpm",
    "assets/icons/64x64/weather-overcast.xpm",
    "assets/icons/64x64/weather-fog.xpm",
    "assets/icons/64x64/weather-showers.xpm",
    "assets/icons/64x64/weather-snow.xpm",
    "assets/icons/64x64/weather-storm.xpm",
};

static Pixmap icon_cache[ICON_COUNT];

typedef struct {
    Widget day_name;
    Widget date;
    Widget icon;
    Widget high;
    Widget low;
} DayColumn;

struct WeatherWidget {
    DayColumn days[FORECAST_DAYS];
};

/* Returns `context`'s current background darkened by `factor` (e.g. 0.85
 * for 15% darker), so the widget's cards stand out from their parent. */
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

/* Maps an Open-Meteo/WMO daily weathercode to an icon. See
 * https://open-meteo.com/en/docs for the full code table. */
static IconKind
icon_kind_for_weather_code(int code)
{
    switch (code) {
    case 0:                                      return ICON_CLEAR;
    case 1: case 2:                               return ICON_FEW_CLOUDS;
    case 3:                                       return ICON_OVERCAST;
    case 45: case 48:                              return ICON_FOG;
    case 51: case 53: case 55: case 56: case 57:
    case 61: case 63: case 65: case 66: case 67:
    case 80: case 81: case 82:                     return ICON_SHOWERS;
    case 71: case 73: case 75: case 77:
    case 85: case 86:                              return ICON_SNOW;
    case 95: case 96: case 99:                     return ICON_STORM;
    default:                                       return ICON_CLEAR;
    }
}

/* Loads (and caches, since there are only ICON_COUNT distinct images) the
 * pixmap for `kind`. Returns XmUNSPECIFIED_PIXMAP if the file is missing. */
static Pixmap
get_icon_pixmap(Widget context, IconKind kind)
{
    if (icon_cache[kind] == 0) {
        Display *dpy = XtDisplay(context);
        Window   root = RootWindowOfScreen(XtScreen(context));
        Pixmap   pixmap, shape_mask;
        int      status;

        status = XpmReadFileToPixmap(dpy, root, (char *)icon_files[kind], &pixmap, &shape_mask, NULL);
        if (status != XpmSuccess) {
            fprintf(stderr, "weather_widget: failed to load icon \"%s\": %s\n",
                    icon_files[kind], XpmGetErrorString(status));
            return XmUNSPECIFIED_PIXMAP;
        }
        if (shape_mask != None)
            XFreePixmap(dpy, shape_mask); /* icons are pre-flattened; no mask needed */

        icon_cache[kind] = pixmap;
    }

    return icon_cache[kind];
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
                                         XmNlabelPixmap, get_icon_pixmap(column, ICON_CLEAR),
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

WeatherWidget *
weather_widget_create(Widget parent)
{
    WeatherWidget *widget = calloc(1, sizeof(*widget));
    Pixel          background = darker_background(parent, 0.85);
    int            i;

    for (i = 0; i < FORECAST_DAYS; i++)
        create_day_column(parent, i, background, &widget->days[i]);

    return widget;
}

void
weather_widget_destroy(WeatherWidget *widget)
{
    free(widget);
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
weather_widget_set_forecast(WeatherWidget *widget, const DailyForecast *days)
{
    int i;

    for (i = 0; i < FORECAST_DAYS; i++) {
        Widget icon = widget->days[i].icon;

        set_label_text(widget->days[i].day_name, days[i].day_name);
        set_label_text(widget->days[i].date, days[i].date_label);
        set_temperature_label(widget->days[i].high, "H: ", days[i].high_c);
        set_temperature_label(widget->days[i].low, "L: ", days[i].low_c);

        if (days[i].weather_code < 0) {
            XtUnmanageChild(icon);
        } else {
            XtVaSetValues(icon, XmNlabelPixmap,
                           get_icon_pixmap(icon, icon_kind_for_weather_code(days[i].weather_code)),
                           NULL);
            XtManageChild(icon);

            /* XmLabel doesn't reliably repaint on its own when only
             * XmNlabelPixmap changes (the resource updates, but the old
             * image stays on screen) - force an expose so it redraws. */
            if (XtIsRealized(icon))
                XClearArea(XtDisplay(icon), XtWindow(icon), 0, 0, 0, 0, True);
        }
    }
}
