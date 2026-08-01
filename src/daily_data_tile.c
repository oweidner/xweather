#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Label.h>

#include "daily_data_tile.h"
#include "weather_icons.h"

/* Motif has no font-independent "bold" resource on XmLabel; weight is only
 * ever expressed as part of a font descriptor. This asks for bold at a
 * larger pixel size and wildcards everything else, so the X server still
 * matches it against the same default font family (here, "Misc Fixed"). */
#define BOLD_FONT "-*-*-bold-r-*-*-18-*-*-*-*-*-*-*"

struct DailyDataTile {
    Widget column; /* the tile's own top-level Form */
    Widget day_name;
    Widget date;
    Widget icon;
    Widget high;
    Widget low;
};

/* Returns `context`'s current background darkened by `factor` (e.g. 0.85
 * for 15% darker), so the tile's card stands out from its parent. Identical
 * helper to hourly_data_tile.c's/hourly_forecast_view.c's darker_background()
 * -- kept as a small, deliberate duplicate rather than a shared module, same
 * as this codebase's existing BOLD_FONT/create_bold_render_table
 * duplication between view.c and this file. */
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

DailyDataTile *
daily_data_tile_create(Widget parent, int index, int count)
{
    DailyDataTile *tile = calloc(1, sizeof(*tile));
    Pixel           bg = darker_background(parent, 0.85);

    tile->column = XtVaCreateManagedWidget("dayColumn", xmFormWidgetClass, parent,
                                            XmNbackground, bg,
                                            XmNshadowThickness, 2,
                                            XmNshadowType, XmSHADOW_ETCHED_IN,
                                            XmNmarginWidth, 6,
                                            XmNmarginHeight, 6,
                                            XmNtopAttachment, XmATTACH_FORM,
                                            XmNbottomAttachment, XmATTACH_FORM,
                                            XmNleftAttachment, XmATTACH_POSITION,
                                            XmNleftPosition, index * 100 / count,
                                            XmNleftOffset, 4,
                                            XmNrightAttachment, XmATTACH_POSITION,
                                            XmNrightPosition, (index + 1) * 100 / count,
                                            XmNrightOffset, 4,
                                            NULL);
    XmChangeColor(tile->column, bg);

    {
        XmRenderTable bold_table = create_bold_render_table(tile->column);

        tile->day_name = XtVaCreateManagedWidget("dayName", xmLabelWidgetClass, tile->column,
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

    tile->date = XtVaCreateManagedWidget("dayDate", xmLabelWidgetClass, tile->column,
                                          XmNbackground, bg,
                                          XmNtopAttachment, XmATTACH_WIDGET,
                                          XmNtopWidget, tile->day_name,
                                          XmNtopOffset, 4,
                                          XmNleftAttachment, XmATTACH_FORM,
                                          XmNrightAttachment, XmATTACH_FORM,
                                          XmNalignment, XmALIGNMENT_CENTER,
                                          NULL);

    tile->icon = XtVaCreateManagedWidget("dayIcon", xmLabelWidgetClass, tile->column,
                                          XmNbackground, bg,
                                          XmNlabelType, XmPIXMAP,
                                          XmNlabelPixmap, weather_icon_for_code(tile->column, 0, 1),
                                          XmNtopAttachment, XmATTACH_WIDGET,
                                          XmNtopWidget, tile->date,
                                          XmNtopOffset, 10,
                                          XmNleftAttachment, XmATTACH_FORM,
                                          XmNrightAttachment, XmATTACH_FORM,
                                          XmNalignment, XmALIGNMENT_CENTER,
                                          NULL);

    {
        XmRenderTable bold_table = create_bold_render_table(tile->column);

        tile->high = XtVaCreateManagedWidget("dayHigh", xmLabelWidgetClass, tile->column,
                                              XmNbackground, bg,
                                              XmNrenderTable, bold_table,
                                              XmNtopAttachment, XmATTACH_WIDGET,
                                              XmNtopWidget, tile->icon,
                                              XmNtopOffset, 10,
                                              XmNleftAttachment, XmATTACH_FORM,
                                              XmNrightAttachment, XmATTACH_FORM,
                                              XmNalignment, XmALIGNMENT_CENTER,
                                              NULL);
        XmRenderTableFree(bold_table);
    }

    tile->low = XtVaCreateManagedWidget("dayLow", xmLabelWidgetClass, tile->column,
                                         XmNbackground, bg,
                                         XmNtopAttachment, XmATTACH_WIDGET,
                                         XmNtopWidget, tile->high,
                                         XmNtopOffset, 8,
                                         XmNleftAttachment, XmATTACH_FORM,
                                         XmNrightAttachment, XmATTACH_FORM,
                                         XmNalignment, XmALIGNMENT_CENTER,
                                         NULL);

    return tile;
}

void
daily_data_tile_destroy(DailyDataTile *tile)
{
    free(tile);
}

void
daily_data_tile_set_data(DailyDataTile *tile, const DailyForecast *day)
{
    Widget icon = tile->icon;

    set_label_text(tile->day_name, day->day_name);
    set_label_text(tile->date, day->date_label);
    set_temperature_label(tile->high, "H: ", day->high_c);
    set_temperature_label(tile->low, "L: ", day->low_c);

    if (day->weather_code < 0) {
        XtUnmanageChild(icon);
    } else {
        XtVaSetValues(icon, XmNlabelPixmap, weather_icon_for_code(icon, day->weather_code, 1), NULL);
        XtManageChild(icon);

        /* XmLabel doesn't reliably repaint on its own when only
         * XmNlabelPixmap changes (the resource updates, but the old image
         * stays on screen) - force an expose so it redraws. */
        if (XtIsRealized(icon))
            XClearArea(XtDisplay(icon), XtWindow(icon), 0, 0, 0, 0, True);
    }
}
