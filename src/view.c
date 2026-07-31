#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <X11/Shell.h>
#include <X11/xpm.h>
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

#define DEFAULT_WINDOW_WIDTH  600
#define DEFAULT_WINDOW_HEIGHT 400

#define ABOUT_DIALOG_WIDTH 260
#define ABOUT_ICON_FILE    "assets/icons/64x64/weather-clear.xpm"
#define ABOUT_VERSION      "Version 0.1-rc1"
#define ABOUT_URL          "https://github.com/oweidner/xweather"

/* See the identical helper in weather_widget.c: Motif has no font-independent
 * "bold" resource on XmLabel, so weight can only be requested as part of a
 * full font descriptor. */
#define BOLD_FONT "-*-*-bold-r-*-*-18-*-*-*-*-*-*-*"

AppView *
view_create(Widget toplevel, const LocationList *locations)
{
    AppView *view = calloc(1, sizeof(*view));
    Widget   menu_bar, file_menu, location_menu, help_menu, help_cascade;
    Widget   work_area, middle_pane;

    view->toplevel = toplevel;

    XtVaSetValues(toplevel,
                  XmNwidth, DEFAULT_WINDOW_WIDTH,
                  XmNheight, DEFAULT_WINDOW_HEIGHT,
                  NULL);

    view->main_window = XmCreateMainWindow(toplevel, "mainWindow", NULL, 0);
    XtManageChild(view->main_window);

    /* Menu bar: File -> Quit, Location -> <one item per LocationList entry>, Help -> About */
    menu_bar = XmCreateMenuBar(view->main_window, "menuBar", NULL, 0);

    file_menu = XmCreatePulldownMenu(menu_bar, "fileMenu", NULL, 0);
    XtVaCreateManagedWidget("File", xmCascadeButtonWidgetClass, menu_bar,
                             XmNsubMenuId, file_menu,
                             XmNmnemonic, 'F',
                             NULL);
    view->quit_item = XtVaCreateManagedWidget("Quit", xmPushButtonWidgetClass, file_menu, NULL);

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

    /* Work area: three horizontal panes (20% / 60% / 20%). The middle pane
     * hosts the forecast widget, which styles its own cards. */
    work_area = XmCreateForm(view->main_window, "workArea", NULL, 0);

    XtVaCreateManagedWidget("topPane", xmFormWidgetClass, work_area,
                             XmNtopAttachment, XmATTACH_FORM,
                             XmNbottomAttachment, XmATTACH_POSITION,
                             XmNbottomPosition, 20,
                             XmNleftAttachment, XmATTACH_FORM,
                             XmNrightAttachment, XmATTACH_FORM,
                             NULL);

    XtVaCreateManagedWidget("bottomPane", xmFormWidgetClass, work_area,
                             XmNtopAttachment, XmATTACH_POSITION,
                             XmNtopPosition, 80,
                             XmNbottomAttachment, XmATTACH_FORM,
                             XmNleftAttachment, XmATTACH_FORM,
                             XmNrightAttachment, XmATTACH_FORM,
                             NULL);

    middle_pane = XtVaCreateManagedWidget("middlePane", xmFormWidgetClass, work_area,
                                           XmNtopAttachment, XmATTACH_POSITION,
                                           XmNtopPosition, 20,
                                           XmNbottomAttachment, XmATTACH_POSITION,
                                           XmNbottomPosition, 80,
                                           XmNleftAttachment, XmATTACH_FORM,
                                           XmNrightAttachment, XmATTACH_FORM,
                                           NULL);

    view->weather_widget = weather_widget_create(middle_pane);

    XtManageChild(work_area);

    XmMainWindowSetAreas(view->main_window, menu_bar, NULL, NULL, NULL, work_area);

    return view;
}

void
view_destroy(AppView *view)
{
    weather_widget_destroy(view->weather_widget);
    free(view);
}

void
view_set_forecast(AppView *view, const char *location, const DailyForecast *days,
                   double current_temperature_c)
{
    view_set_window_title(view, location, current_temperature_c);
    weather_widget_set_forecast(view->weather_widget, days);
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

/* Loads assets/icons/64x64/weather-clear.xpm for the About dialog. Returns
 * XmUNSPECIFIED_PIXMAP if the file is missing, matching how the weather
 * widget's own icon loader degrades. */
static Pixmap
load_about_icon_pixmap(Widget context)
{
    Display *dpy = XtDisplay(context);
    Window   root = RootWindowOfScreen(XtScreen(context));
    Pixmap   pixmap, shape_mask;
    int      status;

    status = XpmReadFileToPixmap(dpy, root, ABOUT_ICON_FILE, &pixmap, &shape_mask, NULL);
    if (status != XpmSuccess) {
        fprintf(stderr, "view: failed to load about icon \"%s\": %s\n",
                ABOUT_ICON_FILE, XpmGetErrorString(status));
        return XmUNSPECIFIED_PIXMAP;
    }
    if (shape_mask != None)
        XFreePixmap(dpy, shape_mask); /* icon is pre-flattened; no mask needed */

    return pixmap;
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
                                              XmNlabelPixmap, load_about_icon_pixmap(view->toplevel),
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
