#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/DrawingA.h>

#include "hourly_data_tile.h"
#include "weather_icons.h"

#define ROW_HEIGHT    40 /* fixed height of the top (time+icon) and bottom (temperature) cells */
#define PANE_SPACING  2  /* gap to the pane's left/right edges within its column */
#define BAR_MARGIN_X  6  /* left/right inset of the bar within the drawing area */
#define BAR_MARGIN_Y  4  /* top/bottom inset of the bar within the drawing area */
#define BAR_COLOR     "#E8A33D" /* warm gold, same accent used by the earlier (reverted) graph */

struct HourlyDataTile {
    Widget pane;        /* the tile's own top-level Form, spans parent's full height */
    Widget top_cell;    /* fixed ROW_HEIGHT: time + icon */
    Widget middle_cell; /* expands to fill the rest */
    Widget bottom_cell; /* fixed ROW_HEIGHT: temperature */
    Widget hour_label;
    Widget icon_label;
    Widget temp_label;
    Widget bar;              /* XmDrawingArea filling middle_cell; height-scaled temperature bar */
    Pixel  day_background;   /* same shade as the pane, used when is_day */
    Pixel  night_background; /* a few shades darker, used when !is_day */
    Pixel  bar_color;        /* allocated once at creation time */

    /* Cached so the bar can be redrawn from an Expose/Resize callback
     * without needing fresh data from the caller. */
    double  temperature_c;
    double  min_c, max_c;
    Boolean has_data;
};

static void
draw_bar(HourlyDataTile *tile)
{
    Display      *dpy = XtDisplay(tile->bar);
    Window        win;
    Dimension     width, height;
    GC            gc;
    double        range, fraction;
    int           usable_height, bar_height, bar_top;

    if (!XtIsRealized(tile->bar))
        return;

    win = XtWindow(tile->bar);
    XtVaGetValues(tile->bar, XmNwidth, &width, XmNheight, &height, NULL);

    /* Clears to the widget's current XmNbackground (already set to the
     * day/night shade in hourly_data_tile_set_data()), so the area above
     * the bar blends with the rest of the tile. */
    XClearWindow(dpy, win);

    if (!tile->has_data || isnan(tile->temperature_c))
        return;

    range = tile->max_c - tile->min_c;
    fraction = (range < 1.0) ? 0.5 : (tile->temperature_c - tile->min_c) / range;
    if (fraction < 0.0) fraction = 0.0;
    if (fraction > 1.0) fraction = 1.0;

    usable_height = (int)height - 2 * BAR_MARGIN_Y;
    if (usable_height < 1)
        return;

    bar_height = (int)(fraction * usable_height);
    if (bar_height < 2)
        bar_height = 2; /* keep even the coldest hour visible as a sliver */
    bar_top = (int)height - BAR_MARGIN_Y - bar_height;

    gc = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc, tile->bar_color);
    XFillRectangle(dpy, win, gc, BAR_MARGIN_X, bar_top,
                   (width > 2 * BAR_MARGIN_X) ? width - 2 * BAR_MARGIN_X : 1, bar_height);
    XFreeGC(dpy, gc);
}

static void
bar_expose_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    HourlyDataTile *tile = (HourlyDataTile *)client_data;

    (void)w;
    (void)call_data;
    draw_bar(tile);
}

static void
bar_resize_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    HourlyDataTile *tile = (HourlyDataTile *)client_data;

    (void)w;
    (void)call_data;
    draw_bar(tile);
}

/* Returns `context`'s current background darkened by `factor` (e.g. 0.85
 * for 15% darker). Identical helper to daily_data_tile.c's/
 * hourly_forecast_view.c's darker_background() -- kept as a small,
 * deliberate duplicate rather than a shared module, same as this
 * codebase's existing BOLD_FONT/create_bold_render_table duplication
 * between view.c and daily_data_tile.c. */
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

HourlyDataTile *
hourly_data_tile_create(Widget parent, int index, int count)
{
    HourlyDataTile *tile = calloc(1, sizeof(*tile));
    Pixel            background = darker_background(parent, 0.85);

    tile->pane = XtVaCreateManagedWidget("hourlyPane", xmFormWidgetClass, parent,
                                          XmNbackground, background,
                                          XmNtopAttachment, XmATTACH_FORM,
                                          XmNbottomAttachment, XmATTACH_FORM,
                                          XmNleftAttachment, XmATTACH_POSITION,
                                          XmNleftPosition, index * 100 / count,
                                          XmNleftOffset, PANE_SPACING,
                                          XmNrightAttachment, XmATTACH_POSITION,
                                          XmNrightPosition, (index + 1) * 100 / count,
                                          XmNrightOffset, PANE_SPACING,
                                          NULL);
    XmChangeColor(tile->pane, background);

    /* Night tiles (is_day == 0, set in hourly_data_tile_set_data()) get a
     * further-darkened shade so they read as visibly "later"/darker at a
     * glance, not just via the moon icon. */
    tile->day_background   = background;
    tile->night_background = darker_background(tile->pane, 0.75);

    {
        Display *dpy = XtDisplay(tile->pane);
        Colormap cmap;
        XColor   color, exact;

        XtVaGetValues(tile->pane, XmNcolormap, &cmap, NULL);
        tile->bar_color = XAllocNamedColor(dpy, cmap, BAR_COLOR, &color, &exact)
                               ? color.pixel : background;
    }

    /* XmForm defaults to XmRESIZE_ANY, which recomputes a form's height
     * from its children and would silently override XmNheight below --
     * pin the policy on the two fixed cells so the height actually sticks
     * (same fix as top_pane/bottom_pane in view.c). The middle cell is left
     * to expand between them, currently just holding an "X" placeholder --
     * destined to hold some kind of per-hour chart element later. */
    tile->top_cell = XtVaCreateManagedWidget("hourlyTopCell", xmFormWidgetClass, tile->pane,
                                              XmNresizePolicy, XmRESIZE_NONE,
                                              XmNbackground, background,
                                              XmNtopAttachment, XmATTACH_FORM,
                                              XmNleftAttachment, XmATTACH_FORM,
                                              XmNrightAttachment, XmATTACH_FORM,
                                              XmNheight, ROW_HEIGHT,
                                              NULL);

    tile->bottom_cell = XtVaCreateManagedWidget("hourlyBottomCell", xmFormWidgetClass, tile->pane,
                                                 XmNresizePolicy, XmRESIZE_NONE,
                                                 XmNbackground, background,
                                                 XmNbottomAttachment, XmATTACH_FORM,
                                                 XmNleftAttachment, XmATTACH_FORM,
                                                 XmNrightAttachment, XmATTACH_FORM,
                                                 XmNheight, ROW_HEIGHT,
                                                 NULL);

    tile->middle_cell = XtVaCreateManagedWidget("hourlyMiddleCell", xmFormWidgetClass, tile->pane,
                                                 XmNbackground, background,
                                                 XmNtopAttachment, XmATTACH_WIDGET,
                                                 XmNtopWidget, tile->top_cell,
                                                 XmNbottomAttachment, XmATTACH_WIDGET,
                                                 XmNbottomWidget, tile->bottom_cell,
                                                 XmNleftAttachment, XmATTACH_FORM,
                                                 XmNrightAttachment, XmATTACH_FORM,
                                                 NULL);

    tile->hour_label = XtVaCreateManagedWidget("hourlyHour", xmLabelWidgetClass, tile->top_cell,
                                                XmNbackground, background,
                                                XmNalignment, XmALIGNMENT_CENTER,
                                                XmNtopAttachment, XmATTACH_FORM,
                                                XmNleftAttachment, XmATTACH_FORM,
                                                XmNrightAttachment, XmATTACH_FORM,
                                                NULL);

    tile->icon_label = XtVaCreateManagedWidget("hourlyIcon", xmLabelWidgetClass, tile->top_cell,
                                                XmNbackground, background,
                                                XmNlabelType, XmPIXMAP,
                                                XmNlabelPixmap, weather_icon_for_code_24(tile->top_cell, 0, 1),
                                                XmNalignment, XmALIGNMENT_CENTER,
                                                XmNtopAttachment, XmATTACH_WIDGET,
                                                XmNtopWidget, tile->hour_label,
                                                XmNleftAttachment, XmATTACH_FORM,
                                                XmNrightAttachment, XmATTACH_FORM,
                                                NULL);

    tile->temp_label = XtVaCreateManagedWidget("hourlyTemp", xmLabelWidgetClass, tile->bottom_cell,
                                                XmNbackground, background,
                                                XmNalignment, XmALIGNMENT_CENTER,
                                                XmNtopAttachment, XmATTACH_FORM,
                                                XmNtopOffset, 10,
                                                XmNleftAttachment, XmATTACH_FORM,
                                                XmNrightAttachment, XmATTACH_FORM,
                                                NULL);

    tile->bar = XtVaCreateManagedWidget("hourlyBar", xmDrawingAreaWidgetClass, tile->middle_cell,
                                         XmNbackground, background,
                                         XmNtopAttachment, XmATTACH_FORM,
                                         XmNbottomAttachment, XmATTACH_FORM,
                                         XmNleftAttachment, XmATTACH_FORM,
                                         XmNrightAttachment, XmATTACH_FORM,
                                         NULL);
    XtAddCallback(tile->bar, XmNexposeCallback, bar_expose_cb, tile);
    XtAddCallback(tile->bar, XmNresizeCallback, bar_resize_cb, tile);

    return tile;
}

void
hourly_data_tile_destroy(HourlyDataTile *tile)
{
    free(tile);
}

void
hourly_data_tile_set_data(HourlyDataTile *tile, const HourlySlot *slot,
                           double min_c, double max_c)
{
    XmString hour_str = XmStringCreateLocalized((char *)slot->hour_label);
    XmString temp_str;
    char     buf[16];
    Pixel    bg = slot->is_day ? tile->day_background : tile->night_background;

    XtVaSetValues(tile->pane, XmNbackground, bg, NULL);
    XmChangeColor(tile->pane, bg);
    XtVaSetValues(tile->top_cell, XmNbackground, bg, NULL);
    XtVaSetValues(tile->middle_cell, XmNbackground, bg, NULL);
    XtVaSetValues(tile->bottom_cell, XmNbackground, bg, NULL);
    XtVaSetValues(tile->hour_label, XmNbackground, bg, NULL);
    XtVaSetValues(tile->icon_label, XmNbackground, bg, NULL);
    XtVaSetValues(tile->temp_label, XmNbackground, bg, NULL);
    XtVaSetValues(tile->bar, XmNbackground, bg, NULL);

    tile->temperature_c = slot->temperature_c;
    tile->min_c          = min_c;
    tile->max_c          = max_c;
    tile->has_data        = True;
    draw_bar(tile);

    XtVaSetValues(tile->hour_label, XmNlabelString, hour_str, NULL);
    XmStringFree(hour_str);

    if (isnan(slot->temperature_c))
        buf[0] = '\0';
    else
        snprintf(buf, sizeof(buf), "%.0f" "\xB0" "C", slot->temperature_c);

    /* Raw Latin-1 byte for '°' -- see the identical rationale next to
     * daily_data_tile.c's set_temperature_label(). */
    temp_str = XmStringCreate(buf, XmFONTLIST_DEFAULT_TAG);
    XtVaSetValues(tile->temp_label, XmNlabelString, temp_str, NULL);
    XmStringFree(temp_str);

    if (slot->weather_code < 0) {
        XtUnmanageChild(tile->icon_label);
    } else {
        XtVaSetValues(tile->icon_label, XmNlabelPixmap,
                       weather_icon_for_code_24(tile->icon_label, slot->weather_code, slot->is_day), NULL);
        XtManageChild(tile->icon_label);

        /* XmLabel doesn't reliably repaint on its own when only
         * XmNlabelPixmap changes -- force an expose so it redraws. Same
         * workaround as daily_data_tile.c. */
        if (XtIsRealized(tile->icon_label))
            XClearArea(XtDisplay(tile->icon_label), XtWindow(tile->icon_label), 0, 0, 0, 0, True);
    }
}
