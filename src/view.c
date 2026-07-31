#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <X11/Shell.h>
#include <Xm/Xm.h>
#include <Xm/MainW.h>
#include <Xm/RowColumn.h>
#include <Xm/CascadeB.h>
#include <Xm/PushB.h>
#include <Xm/Label.h>
#include <Xm/Protocols.h>
#include <Xm/Separator.h>
#include <Xm/ToggleB.h>
#include <Xm/Form.h>

#include "view.h"
#include "weather_icons.h"

static XmRenderTable create_bold_render_table(Widget context);

#define DEFAULT_WINDOW_WIDTH  600
#define DEFAULT_WINDOW_HEIGHT 400

#define TOP_PANE_HEIGHT    80
#define BOTTOM_PANE_HEIGHT 20

#define CURRENT_ICON_SIZE 64
#define CURRENT_ICON_GAP  16

#define ABOUT_DIALOG_WIDTH 260
#define ABOUT_VERSION      "Version 0.1-rc1"
#define ABOUT_URL          "https://github.com/oweidner/xweather"

/* See the identical helper in daily_forecast_view.c: Motif has no
 * font-independent "bold" resource on XmLabel, so weight can only be
 * requested as part of a full font descriptor. */
#define BOLD_FONT "-*-*-bold-r-*-*-18-*-*-*-*-*-*-*"

AppView *
view_create(Widget toplevel, const LocationList *locations)
{
    AppView *view = calloc(1, sizeof(*view));
    Widget   menu_bar, file_menu, view_menu, location_menu, help_menu, help_cascade;
    Widget   outer_form, status_separator, work_area, top_pane, middle_pane, bottom_pane;

    view->toplevel = toplevel;

    XtVaSetValues(toplevel,
                  XmNwidth, DEFAULT_WINDOW_WIDTH,
                  XmNheight, DEFAULT_WINDOW_HEIGHT,
                  NULL);

    view->main_window = XmCreateMainWindow(toplevel, "mainWindow", NULL, 0);
    XtManageChild(view->main_window);

    /* Menu bar: File -> Quit, View -> Hourly/5-Day Forecast,
     * Location -> <one item per LocationList entry>, Help -> About */
    menu_bar = XmCreateMenuBar(view->main_window, "menuBar", NULL, 0);

    file_menu = XmCreatePulldownMenu(menu_bar, "fileMenu", NULL, 0);
    XtVaCreateManagedWidget("File", xmCascadeButtonWidgetClass, menu_bar,
                             XmNsubMenuId, file_menu,
                             XmNmnemonic, 'F',
                             NULL);

    {
        XmString label = XmStringCreateLocalized("Quit");
        XmString accel_text = XmStringCreateLocalized("Ctrl+Q");

        view->quit_item = XtVaCreateManagedWidget("Quit", xmPushButtonWidgetClass, file_menu,
                                                    XmNlabelString, label,
                                                    XmNaccelerator, "Ctrl<Key>Q",
                                                    XmNacceleratorText, accel_text,
                                                    NULL);
        XmStringFree(label);
        XmStringFree(accel_text);
    }

    view_menu = XmCreatePulldownMenu(menu_bar, "viewMenu", NULL, 0);
    XtVaSetValues(view_menu, XmNradioBehavior, True, XmNradioAlwaysOne, True, NULL);
    XtVaCreateManagedWidget("View", xmCascadeButtonWidgetClass, menu_bar,
                             XmNsubMenuId, view_menu,
                             XmNmnemonic, 'V',
                             NULL);

    {
        XmString label = XmStringCreateLocalized("Hourly Forecast");
        XmString accel_text = XmStringCreateLocalized("Ctrl+H");

        view->hourly_forecast_item = XtVaCreateManagedWidget("hourlyForecastItem",
                                                               xmToggleButtonWidgetClass, view_menu,
                                                               XmNlabelString, label,
                                                               XmNindicatorType, XmONE_OF_MANY,
                                                               XmNset, False,
                                                               XmNaccelerator, "Ctrl<Key>H",
                                                               XmNacceleratorText, accel_text,
                                                               NULL);
        XmStringFree(label);
        XmStringFree(accel_text);
    }

    {
        XmString label = XmStringCreateLocalized("5-Day Forecast");
        XmString accel_text = XmStringCreateLocalized("Ctrl+D");

        view->five_day_forecast_item = XtVaCreateManagedWidget("fiveDayForecastItem",
                                                                 xmToggleButtonWidgetClass, view_menu,
                                                                 XmNlabelString, label,
                                                                 XmNindicatorType, XmONE_OF_MANY,
                                                                 XmNset, True,
                                                                 XmNaccelerator, "Ctrl<Key>D",
                                                                 XmNacceleratorText, accel_text,
                                                                 NULL);
        XmStringFree(label);
        XmStringFree(accel_text);
    }

    location_menu = XmCreatePulldownMenu(menu_bar, "locationMenu", NULL, 0);
    XtVaSetValues(location_menu, XmNradioBehavior, True, XmNradioAlwaysOne, True, NULL);
    XtVaCreateManagedWidget("Location", xmCascadeButtonWidgetClass, menu_bar,
                             XmNsubMenuId, location_menu,
                             XmNmnemonic, 'L',
                             NULL);

    {
        XmString label = XmStringCreateLocalized("Manage...");
        XmString accel_text = XmStringCreateLocalized("Ctrl+M");

        view->manage_locations_item = XtVaCreateManagedWidget("manageLocationsItem",
                                                                xmPushButtonWidgetClass, location_menu,
                                                                XmNlabelString, label,
                                                                XmNaccelerator, "Ctrl<Key>M",
                                                                XmNacceleratorText, accel_text,
                                                                NULL);
        XmStringFree(label);
        XmStringFree(accel_text);
    }

    XtVaCreateManagedWidget("locationSeparator", xmSeparatorWidgetClass, location_menu, NULL);

    view->num_location_items = location_list_count(locations);
    {
        int i;

        for (i = 0; i < view->num_location_items; i++) {
            const Location *loc = location_list_get(locations, i);

            view->location_items[i] = XtVaCreateManagedWidget(loc->name, xmToggleButtonWidgetClass,
                                                                location_menu,
                                                                XmNindicatorType, XmONE_OF_MANY,
                                                                XmNset, i == 0,
                                                                NULL);
        }
    }

    help_menu = XmCreatePulldownMenu(menu_bar, "helpMenu", NULL, 0);
    help_cascade = XtVaCreateManagedWidget("Help", xmCascadeButtonWidgetClass, menu_bar,
                                            XmNsubMenuId, help_menu,
                                            NULL);
    view->about_item = XtVaCreateManagedWidget("About", xmPushButtonWidgetClass, help_menu, NULL);

    /* Right-flush the Help cascade, per Motif convention. */
    XtVaSetValues(menu_bar, XmNmenuHelpWidget, help_cascade, NULL);
    XtManageChild(menu_bar);

    /* Outer form: the forecast work area on top, a fixed status bar (for
     * "Last updated" / data-source messages) pinned to the bottom. */
    outer_form = XmCreateForm(view->main_window, "outerForm", NULL, 0);

    /* Status bar: three independently-updatable thirds, sharing the same
     * bottom row -- left-aligned, center-aligned, and right-aligned. */
    view->status_left = XtVaCreateManagedWidget("statusLeft", xmLabelWidgetClass, outer_form,
                                                 XmNalignment, XmALIGNMENT_BEGINNING,
                                                 XmNmarginWidth, 8,
                                                 XmNmarginHeight, 4,
                                                 XmNbottomAttachment, XmATTACH_FORM,
                                                 XmNleftAttachment, XmATTACH_FORM,
                                                 XmNrightAttachment, XmATTACH_POSITION,
                                                 XmNrightPosition, 33,
                                                 NULL);

    view->status_middle = XtVaCreateManagedWidget("statusMiddle", xmLabelWidgetClass, outer_form,
                                                   XmNalignment, XmALIGNMENT_CENTER,
                                                   XmNmarginWidth, 8,
                                                   XmNmarginHeight, 4,
                                                   XmNbottomAttachment, XmATTACH_FORM,
                                                   XmNleftAttachment, XmATTACH_POSITION,
                                                   XmNleftPosition, 33,
                                                   XmNrightAttachment, XmATTACH_POSITION,
                                                   XmNrightPosition, 66,
                                                   NULL);

    view->status_right = XtVaCreateManagedWidget("statusRight", xmLabelWidgetClass, outer_form,
                                                  XmNalignment, XmALIGNMENT_END,
                                                  XmNmarginWidth, 8,
                                                  XmNmarginHeight, 4,
                                                  XmNbottomAttachment, XmATTACH_FORM,
                                                  XmNleftAttachment, XmATTACH_POSITION,
                                                  XmNleftPosition, 66,
                                                  XmNrightAttachment, XmATTACH_FORM,
                                                  NULL);

    status_separator = XtVaCreateManagedWidget("statusSeparator", xmSeparatorWidgetClass, outer_form,
                                                XmNbottomAttachment, XmATTACH_WIDGET,
                                                XmNbottomWidget, view->status_left,
                                                XmNleftAttachment, XmATTACH_FORM,
                                                XmNrightAttachment, XmATTACH_FORM,
                                                NULL);

    /* Work area: a fixed-height top pane for the current-conditions summary
     * and a fixed-height (placeholder) bottom pane, with the forecast pane
     * filling whatever space remains between them. The middle pane hosts
     * the forecast widget, which styles its own cards. */
    work_area = XmCreateForm(outer_form, "workArea", NULL, 0);
    XtVaSetValues(work_area,
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNbottomAttachment, XmATTACH_WIDGET,
                  XmNbottomWidget, status_separator,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_FORM,
                  NULL);

    /* XmForm defaults to XmRESIZE_ANY, which recomputes its own height from
     * its children's attachments (here, currentIcon's top+bottom ATTACH_FORM)
     * and would silently override XmNheight below. Pin the policy so the
     * fixed height actually sticks. */
    top_pane = XtVaCreateManagedWidget("topPane", xmFormWidgetClass, work_area,
                                        XmNresizePolicy, XmRESIZE_NONE,
                                        XmNtopAttachment, XmATTACH_FORM,
                                        XmNleftAttachment, XmATTACH_FORM,
                                        XmNrightAttachment, XmATTACH_FORM,
                                        XmNheight, TOP_PANE_HEIGHT,
                                        NULL);

    /* Current-conditions summary: icon on the left, location name and
     * temperature stacked to its right. currentLocation/currentTemperature
     * ride along, anchored to currentIcon via XmATTACH_WIDGET. */
    view->current_icon = XtVaCreateManagedWidget("currentIcon", xmLabelWidgetClass, top_pane,
                                                  XmNlabelType, XmPIXMAP,
                                                  XmNlabelPixmap, weather_icon_for_code_64(top_pane, 0),
                                                  XmNtopAttachment, XmATTACH_FORM,
                                                  XmNtopOffset, (TOP_PANE_HEIGHT - CURRENT_ICON_SIZE) / 2,
                                                  XmNleftAttachment, XmATTACH_FORM,
                                                  XmNleftOffset, 14,
                                                  NULL);

    {
        XmRenderTable bold_table = create_bold_render_table(top_pane);

        view->current_location = XtVaCreateManagedWidget("currentLocation", xmLabelWidgetClass, top_pane,
                                                           XmNrenderTable, bold_table,
                                                           XmNtopAttachment, XmATTACH_FORM,
                                                           XmNtopOffset, 18,
                                                           XmNleftAttachment, XmATTACH_WIDGET,
                                                           XmNleftWidget, view->current_icon,
                                                           XmNleftOffset, CURRENT_ICON_GAP,
                                                           NULL);
        view->current_temperature = XtVaCreateManagedWidget("currentTemperature", xmLabelWidgetClass,
                                                              top_pane,
                                                              XmNrenderTable, bold_table,
                                                              XmNtopAttachment, XmATTACH_WIDGET,
                                                              XmNtopWidget, view->current_location,
                                                              XmNtopOffset, 2,
                                                              XmNleftAttachment, XmATTACH_WIDGET,
                                                              XmNleftWidget, view->current_icon,
                                                              XmNleftOffset, CURRENT_ICON_GAP,
                                                              NULL);
        XmRenderTableFree(bold_table);
    }

    bottom_pane = XtVaCreateManagedWidget("bottomPane", xmFormWidgetClass, work_area,
                                           XmNresizePolicy, XmRESIZE_NONE,
                                           XmNbottomAttachment, XmATTACH_FORM,
                                           XmNleftAttachment, XmATTACH_FORM,
                                           XmNrightAttachment, XmATTACH_FORM,
                                           XmNheight, BOTTOM_PANE_HEIGHT,
                                           NULL);

    middle_pane = XtVaCreateManagedWidget("middlePane", xmFormWidgetClass, work_area,
                                           XmNtopAttachment, XmATTACH_WIDGET,
                                           XmNtopWidget, top_pane,
                                           XmNbottomAttachment, XmATTACH_WIDGET,
                                           XmNbottomWidget, bottom_pane,
                                           XmNleftAttachment, XmATTACH_FORM,
                                           XmNleftOffset, 10,
                                           XmNrightAttachment, XmATTACH_FORM,
                                           XmNrightOffset, 10,
                                           NULL);

    view->daily_forecast_view = daily_forecast_view_create(middle_pane);
    view->hourly_forecast_view = hourly_forecast_view_create(middle_pane);
    XtUnmanageChild(hourly_forecast_view_widget(view->hourly_forecast_view)); /* 5-Day Forecast is the default */

    XtManageChild(work_area);
    XtManageChild(outer_form);

    XmMainWindowSetAreas(view->main_window, menu_bar, NULL, NULL, NULL, outer_form);

    return view;
}

void
view_destroy(AppView *view)
{
    daily_forecast_view_destroy(view->daily_forecast_view);
    hourly_forecast_view_destroy(view->hourly_forecast_view);
    free(view);
}

void
view_show_daily_forecast(AppView *view)
{
    XtUnmanageChild(hourly_forecast_view_widget(view->hourly_forecast_view));
    XtManageChild(daily_forecast_view_widget(view->daily_forecast_view));
}

void
view_show_hourly_forecast(AppView *view)
{
    XtUnmanageChild(daily_forecast_view_widget(view->daily_forecast_view));
    XtManageChild(hourly_forecast_view_widget(view->hourly_forecast_view));
}

/* Our labels render through plain core X fonts (ISO8859-1), which can't
 * display multi-byte UTF-8 text. XmStringCreate (unlike ...Localized) takes
 * the bytes as-is for the given charset tag, with no locale-driven
 * reinterpretation, so this can hand it the raw Latin-1 byte for '°'. See
 * the identical helper in daily_forecast_view.c. */
static XmString
make_temperature_string(double temperature_c)
{
    char buf[32];

    if (isnan(temperature_c))
        buf[0] = '\0';
    else
        snprintf(buf, sizeof(buf), "%.0f" "\xB0" "C", temperature_c);

    return XmStringCreate(buf, XmFONTLIST_DEFAULT_TAG);
}

static void
set_current_icon(AppView *view, int weather_code)
{
    if (weather_code < 0) {
        XtUnmanageChild(view->current_icon);
        return;
    }

    XtVaSetValues(view->current_icon, XmNlabelPixmap,
                   weather_icon_for_code_64(view->current_icon, weather_code), NULL);
    XtManageChild(view->current_icon);

    /* XmLabel doesn't reliably repaint on its own when only XmNlabelPixmap
     * changes (the resource updates, but the old image stays on screen) --
     * force an expose so it redraws. Same workaround as daily_forecast_view.c. */
    if (XtIsRealized(view->current_icon))
        XClearArea(XtDisplay(view->current_icon), XtWindow(view->current_icon), 0, 0, 0, 0, True);
}

void
view_set_forecast(AppView *view, const char *location, const DailyForecast *days,
                   double current_temperature_c)
{
    XmString location_str = XmStringCreateLocalized((char *)location);
    XmString temperature_str = make_temperature_string(current_temperature_c);

    view_set_window_title(view, location, current_temperature_c);

    XtVaSetValues(view->current_location, XmNlabelString, location_str, NULL);
    XtVaSetValues(view->current_temperature, XmNlabelString, temperature_str, NULL);

    XmStringFree(location_str);
    XmStringFree(temperature_str);

    set_current_icon(view, days[0].weather_code);

    daily_forecast_view_set_forecast(view->daily_forecast_view, days);
}

void
view_set_status(AppView *view, StatusArea area, const char *text)
{
    XmString str = XmStringCreateLocalized((char *)text);
    Widget   label;

    switch (area) {
    case STATUS_AREA_LEFT:   label = view->status_left;   break;
    case STATUS_AREA_MIDDLE: label = view->status_middle; break;
    case STATUS_AREA_RIGHT:  label = view->status_right;  break;
    default:                 label = view->status_left;   break;
    }

    XtVaSetValues(label, XmNlabelString, str, NULL);
    XmStringFree(str);
}

/* current_temperature_c is NAN to omit the "(..°C)" suffix (e.g. right
 * after picking a Location we haven't fetched data for yet). Unlike our
 * XmString labels (which bypass locale via XmStringCreate and need a raw
 * Latin-1 byte), setting XtNtitle goes through Xt's locale-aware WM
 * property conversion, so it wants real UTF-8 here instead. */
void
view_set_window_title(AppView *view, const char *location, double current_temperature_c)
{
    char title[192];

    if (isnan(current_temperature_c))
        snprintf(title, sizeof(title), "XWeather - %s", location);
    else
        snprintf(title, sizeof(title), "XWeather - %s (%.0f" "\xC2\xB0" "C)",
                 location, current_temperature_c);

    XtVaSetValues(view->toplevel, XtNtitle, title, NULL);
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

static void
on_about_ok(Widget w, XtPointer client_data, XtPointer call_data)
{
    (void)w;
    (void)call_data;
    XtUnmanageChild((Widget)client_data);
}

void
view_show_about_dialog(AppView *view)
{
    if (!view->about_dialog) {
        Widget        icon_label, name_label, version_label, link_label, separator, ok_button;
        XmRenderTable bold_table;
        XmString      str;
        Atom          wm_delete_window;

        view->about_dialog = XmCreateFormDialog(view->toplevel, "aboutDialog", NULL, 0);
        XtVaSetValues(view->about_dialog,
                      XmNdialogStyle, XmDIALOG_APPLICATION_MODAL,
                      XmNautoUnmanage, False,
                      XmNwidth, ABOUT_DIALOG_WIDTH,
                      NULL);
        XtVaSetValues(XtParent(view->about_dialog), XtNtitle, "About XWeather", NULL);

        icon_label = XtVaCreateManagedWidget("aboutIcon", xmLabelWidgetClass, view->about_dialog,
                                              XmNlabelType, XmPIXMAP,
                                              XmNlabelPixmap, weather_icon_for_code_64(view->about_dialog, 0),
                                              XmNtopAttachment, XmATTACH_FORM,
                                              XmNtopOffset, 20,
                                              XmNleftAttachment, XmATTACH_FORM,
                                              XmNrightAttachment, XmATTACH_FORM,
                                              XmNalignment, XmALIGNMENT_CENTER,
                                              NULL);

        bold_table = create_bold_render_table(view->about_dialog);
        str = XmStringCreateLocalized("XWeather");
        name_label = XtVaCreateManagedWidget("aboutName", xmLabelWidgetClass, view->about_dialog,
                                              XmNlabelString, str,
                                              XmNrenderTable, bold_table,
                                              XmNtopAttachment, XmATTACH_WIDGET,
                                              XmNtopWidget, icon_label,
                                              XmNtopOffset, 12,
                                              XmNleftAttachment, XmATTACH_FORM,
                                              XmNrightAttachment, XmATTACH_FORM,
                                              XmNalignment, XmALIGNMENT_CENTER,
                                              NULL);
        XmStringFree(str);
        XmRenderTableFree(bold_table);

        str = XmStringCreateLocalized(ABOUT_VERSION);
        version_label = XtVaCreateManagedWidget("aboutVersion", xmLabelWidgetClass, view->about_dialog,
                                                 XmNlabelString, str,
                                                 XmNtopAttachment, XmATTACH_WIDGET,
                                                 XmNtopWidget, name_label,
                                                 XmNtopOffset, 4,
                                                 XmNleftAttachment, XmATTACH_FORM,
                                                 XmNrightAttachment, XmATTACH_FORM,
                                                 XmNalignment, XmALIGNMENT_CENTER,
                                                 NULL);
        XmStringFree(str);

        str = XmStringCreateLocalized(ABOUT_URL);
        link_label = XtVaCreateManagedWidget("aboutLink", xmLabelWidgetClass, view->about_dialog,
                                              XmNlabelString, str,
                                              XmNtopAttachment, XmATTACH_WIDGET,
                                              XmNtopWidget, version_label,
                                              XmNtopOffset, 12,
                                              XmNleftAttachment, XmATTACH_FORM,
                                              XmNrightAttachment, XmATTACH_FORM,
                                              XmNalignment, XmALIGNMENT_CENTER,
                                              NULL);
        XmStringFree(str);

        separator = XtVaCreateManagedWidget("aboutSeparator", xmSeparatorWidgetClass, view->about_dialog,
                                             XmNtopAttachment, XmATTACH_WIDGET,
                                             XmNtopWidget, link_label,
                                             XmNtopOffset, 16,
                                             XmNleftAttachment, XmATTACH_FORM,
                                             XmNrightAttachment, XmATTACH_FORM,
                                             NULL);

        str = XmStringCreateLocalized("OK");
        ok_button = XtVaCreateManagedWidget("aboutOkButton", xmPushButtonWidgetClass, view->about_dialog,
                                             XmNlabelString, str,
                                             XmNtopAttachment, XmATTACH_WIDGET,
                                             XmNtopWidget, separator,
                                             XmNtopOffset, 12,
                                             XmNbottomAttachment, XmATTACH_FORM,
                                             XmNbottomOffset, 16,
                                             XmNleftAttachment, XmATTACH_POSITION,
                                             XmNleftPosition, 35,
                                             XmNrightAttachment, XmATTACH_POSITION,
                                             XmNrightPosition, 65,
                                             NULL);
        XmStringFree(str);
        XtAddCallback(ok_button, XmNactivateCallback, on_about_ok, view->about_dialog);

        /* Make the dialog's own window-manager close button behave like OK. */
        wm_delete_window = XmInternAtom(XtDisplay(view->toplevel), "WM_DELETE_WINDOW", False);
        XmAddWMProtocols(XtParent(view->about_dialog), &wm_delete_window, 1);
        XmAddWMProtocolCallback(XtParent(view->about_dialog), wm_delete_window,
                                 on_about_ok, view->about_dialog);
    }

    XtManageChild(view->about_dialog);
}
