#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <X11/Shell.h>
#include <X11/xpm.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/Protocols.h>
#include <Xm/PushB.h>
#include <Xm/RowColumn.h>
#include <Xm/ScrolledW.h>
#include <Xm/TextF.h>
#include <Xm/ToggleB.h>

#include "ascii_sanitize.h"
#include "location_manager_view.h"
#include "weather_client.h" /* GeocodeResult, weather_client_geocode_search */

#define WINDOW_WIDTH  380
#define WINDOW_HEIGHT 600

/* Fixed height of the search-results list, so it doesn't compete with the
 * staged-locations list below it for the form's stretchy space -- see the
 * layout comment in location_manager_view_create(). */
#define RESULTS_AREA_HEIGHT 130

/* Fixed width of the checkbox column in each location row -- the name
 * column fills whatever's left. */
#define CHECKBOX_COLUMN_WIDTH 34

/* Motif derives XmNindicatorSize from the font by default, which comes out
 * small enough to be hard to hit -- pick a fixed size instead. */
#define CHECKBOX_INDICATOR_SIZE 20

/* Same icon file as the main window (see view.c's set_wm_icon) -- duplicated
 * rather than shared, same as this codebase's existing BOLD_FONT/
 * create_bold_render_table duplication between view.c and daily_data_tile.c. */
#define APP_ICON_FILE "assets/icons/64x64/weather-clear.xpm"

struct LocationManagerView {
    Widget shell;
    Widget search_field;
    Widget search_button;
    Widget results_column;
    Widget add_location_button;
    Widget row_column;
    Widget remove_button;
    Widget apply_button;
    Widget cancel_button;

    /* The in-window copy being edited -- Add/remove only touch this, never
     * anything outside the window. Reset from the live LocationList every
     * time the window is shown; only Apply pushes it back out. */
    ConfigLocation staged[MAX_LOCATIONS];
    int            staged_count;

    /* Per-row widgets, parallel to staged[0..rows_built-1], so refresh_rows()
     * knows what to destroy and on_remove_clicked() can read each row's
     * checked state by walking row_checks[] -- same pattern view.c's
     * location_items[]/on_location_selected() uses. */
    Widget row_forms[MAX_LOCATIONS];
    Widget row_checks[MAX_LOCATIONS];
    int    rows_built;

    /* Results of the last Search click, separate from staged[] until "Add
     * Location" moves the checked ones over. Same parallel-widget-array
     * pattern as row_forms/row_checks above. */
    GeocodeResult search_results[MAX_GEOCODE_RESULTS];
    int           search_result_count;
    Widget        result_forms[MAX_GEOCODE_RESULTS];
    Widget        result_checks[MAX_GEOCODE_RESULTS];
    int           result_rows_built;

    LocationManagerApplyFn on_apply;
    void                   *user_data;
};

static void refresh_rows(LocationManagerView *view);
static void refresh_result_rows(LocationManagerView *view);

static void
set_wm_icon(Widget shell)
{
    Display *dpy = XtDisplay(shell);
    Window   root = RootWindowOfScreen(XtScreen(shell));
    Pixmap   icon_pixmap, icon_mask;
    int      status;

    status = XpmReadFileToPixmap(dpy, root, APP_ICON_FILE, &icon_pixmap, &icon_mask, NULL);
    if (status != XpmSuccess) {
        fprintf(stderr, "location_manager_view: failed to load app icon \"%s\": %s\n",
                APP_ICON_FILE, XpmGetErrorString(status));
        return;
    }

    XtVaSetValues(shell,
                  XtNiconPixmap, icon_pixmap,
                  XtNiconMask, icon_mask,
                  NULL);
}

/* Neither the results list nor the staged-locations list ever needs
 * horizontal scrolling -- their rows track the scroll area's width, and the
 * name column already gets whatever room the checkbox column leaves it.
 * Only the vertical scrollbar (always shown via XmNscrollBarDisplayPolicy)
 * should stick around. */
static void
unmanage_horizontal_scrollbar(Widget scrolled_window)
{
    Widget hsb;

    XtVaGetValues(scrolled_window, XmNhorizontalScrollBar, &hsb, NULL);
    if (hsb)
        XtUnmanageChild(hsb);
}

/* Shared by the Cancel button, the window's own close button, and the WM's
 * close protocol -- none of them need to touch the staged list, since
 * location_manager_view_show() resets it from the live LocationList again
 * next time the window is opened. */
static void
on_close(Widget w, XtPointer client_data, XtPointer call_data)
{
    (void)w;
    (void)call_data;
    XtPopdown((Widget)client_data);
}

/* Reflects whether any row is currently checked onto the Remove button's
 * sensitivity. Called after every checkbox toggle and every refresh_rows(),
 * since rebuilding the rows always starts them unchecked. */
static void
update_remove_sensitivity(LocationManagerView *view)
{
    int i;

    for (i = 0; i < view->staged_count; i++) {
        if (XmToggleButtonGetState(view->row_checks[i])) {
            XtSetSensitive(view->remove_button, True);
            return;
        }
    }

    XtSetSensitive(view->remove_button, False);
}

/* XmNvalueChangedCallback for each row's checkbox. */
static void
on_row_check_changed(Widget w, XtPointer client_data, XtPointer call_data)
{
    (void)w;
    (void)call_data;
    update_remove_sensitivity((LocationManagerView *)client_data);
}

/* Fired by the Remove button below the scroll view. Compacts staged[] down
 * to the rows that were left unchecked, preserving order, then rebuilds the
 * list. A no-op if nothing is checked. */
static void
on_remove_clicked(Widget w, XtPointer client_data, XtPointer call_data)
{
    LocationManagerView *view = (LocationManagerView *)client_data;
    int                   i, kept;

    (void)w;
    (void)call_data;

    for (i = 0, kept = 0; i < view->staged_count; i++) {
        if (XmToggleButtonGetState(view->row_checks[i]))
            continue;

        if (kept != i)
            view->staged[kept] = view->staged[i];
        kept++;
    }

    if (kept == view->staged_count)
        return;

    view->staged_count = kept;
    refresh_rows(view);
}

/* Destroys and rebuilds every row from view->staged[], and updates the Add
 * button's sensitivity for the new count. Called after every staged-list
 * change (add, remove, or a fresh location_manager_view_show()). */
static void
refresh_rows(LocationManagerView *view)
{
    int i;

    for (i = 0; i < view->rows_built; i++)
        XtDestroyWidget(view->row_forms[i]); /* also destroys the row's checkbox */

    for (i = 0; i < view->staged_count; i++) {
        Widget   row, check;
        XmString str;
        char     ascii_query[196];

        row = XtVaCreateManagedWidget("locationRow", xmFormWidgetClass, view->row_column, NULL);

        /* Column 1: a fixed-width, unlabeled checkbox -- XmNalignment centers
         * the indicator within XmNwidth since there's no label string to
         * share the space with. */
        str = XmStringCreateLocalized("");
        check = XtVaCreateManagedWidget("locationCheck", xmToggleButtonWidgetClass, row,
                                         XmNlabelString, str,
                                         XmNindicatorType, XmN_OF_MANY,
                                         XmNindicatorSize, CHECKBOX_INDICATOR_SIZE,
                                         XmNset, False,
                                         XmNalignment, XmALIGNMENT_CENTER,
                                         XmNmarginWidth, 0,
                                         XmNwidth, CHECKBOX_COLUMN_WIDTH,
                                         XmNtopAttachment, XmATTACH_FORM,
                                         XmNbottomAttachment, XmATTACH_FORM,
                                         XmNleftAttachment, XmATTACH_FORM,
                                         NULL);
        XmStringFree(str);
        XtAddCallback(check, XmNvalueChangedCallback, on_row_check_changed, view);

        /* Column 2: the full location, filling the rest of the row. name and
         * query are always identical (see on_add_location_clicked()), so
         * either would do -- query is the more semantically apt one to show
         * here. Sanitized, like every other Motif label in this app, since
         * it can't render arbitrary Unicode glyphs. */
        ascii_sanitize(view->staged[i].query, ascii_query, sizeof(ascii_query));
        str = XmStringCreateLocalized(ascii_query);
        XtVaCreateManagedWidget("locationName", xmLabelWidgetClass, row,
                                 XmNlabelString, str,
                                 XmNalignment, XmALIGNMENT_BEGINNING,
                                 XmNtopAttachment, XmATTACH_FORM,
                                 XmNbottomAttachment, XmATTACH_FORM,
                                 XmNleftAttachment, XmATTACH_WIDGET,
                                 XmNleftWidget, check,
                                 XmNleftOffset, 6,
                                 XmNrightAttachment, XmATTACH_FORM,
                                 XmNrightOffset, 6,
                                 NULL);
        XmStringFree(str);

        view->row_forms[i]  = row;
        view->row_checks[i] = check;
    }

    view->rows_built = view->staged_count;

    update_remove_sensitivity(view);
}

/* Destroys and rebuilds every row from view->search_results[]. Mirrors
 * refresh_rows()'s checkbox-column layout, but for search candidates rather
 * than staged locations -- there's no per-row remove here, just the bulk
 * "Add Location" button below the list. */
static void
refresh_result_rows(LocationManagerView *view)
{
    int i;

    for (i = 0; i < view->result_rows_built; i++)
        XtDestroyWidget(view->result_forms[i]);

    for (i = 0; i < view->search_result_count; i++) {
        Widget   row, check;
        XmString str;
        char     label[196];
        char     ascii_label[196];

        row = XtVaCreateManagedWidget("resultRow", xmFormWidgetClass, view->results_column, NULL);

        str = XmStringCreateLocalized("");
        check = XtVaCreateManagedWidget("resultCheck", xmToggleButtonWidgetClass, row,
                                         XmNlabelString, str,
                                         XmNindicatorType, XmN_OF_MANY,
                                         XmNindicatorSize, CHECKBOX_INDICATOR_SIZE,
                                         XmNset, False,
                                         XmNalignment, XmALIGNMENT_CENTER,
                                         XmNmarginWidth, 0,
                                         XmNwidth, CHECKBOX_COLUMN_WIDTH,
                                         XmNtopAttachment, XmATTACH_FORM,
                                         XmNbottomAttachment, XmATTACH_FORM,
                                         XmNleftAttachment, XmATTACH_FORM,
                                         NULL);
        XmStringFree(str);

        weather_client_geocode_format(&view->search_results[i], label, sizeof(label));
        ascii_sanitize(label, ascii_label, sizeof(ascii_label));
        str = XmStringCreateLocalized(ascii_label);
        XtVaCreateManagedWidget("resultName", xmLabelWidgetClass, row,
                                 XmNlabelString, str,
                                 XmNalignment, XmALIGNMENT_BEGINNING,
                                 XmNtopAttachment, XmATTACH_FORM,
                                 XmNbottomAttachment, XmATTACH_FORM,
                                 XmNleftAttachment, XmATTACH_WIDGET,
                                 XmNleftWidget, check,
                                 XmNleftOffset, 6,
                                 XmNrightAttachment, XmATTACH_FORM,
                                 XmNrightOffset, 6,
                                 NULL);
        XmStringFree(str);

        view->result_forms[i]  = row;
        view->result_checks[i] = check;
    }

    view->result_rows_built = view->search_result_count;
}

/* Inserts name/query into view->staged[] in case-insensitive name order --
 * same insertion-sort technique locations.c's location_list_add() uses, so
 * the list stays alphabetized as the user builds it up, matching how the
 * rest of the app already presents locations. Caller must have already
 * checked staged_count < MAX_LOCATIONS. */
static void
insert_staged_sorted(LocationManagerView *view, const char *name, const char *query)
{
    int insert_at, k;

    for (insert_at = 0; insert_at < view->staged_count; insert_at++) {
        if (strcasecmp(name, view->staged[insert_at].name) < 0)
            break;
    }

    for (k = view->staged_count; k > insert_at; k--)
        view->staged[k] = view->staged[k - 1];

    strncpy(view->staged[insert_at].name, name, sizeof(view->staged[insert_at].name) - 1);
    view->staged[insert_at].name[sizeof(view->staged[insert_at].name) - 1] = '\0';
    strncpy(view->staged[insert_at].query, query, sizeof(view->staged[insert_at].query) - 1);
    view->staged[insert_at].query[sizeof(view->staged[insert_at].query) - 1] = '\0';

    view->staged_count++;
}

/* Fired by both the Search button and the search field's Enter key. Reads
 * and trims the search field, then runs the geocoding lookup synchronously
 * on the Xt main thread -- a brief UI freeze during the request, acceptable
 * for now since this dialog has no background-fetch machinery of its own
 * (unlike the main window's fetch_manager). Replaces whatever results list
 * was already showing; an empty field or a failed/empty search just clears
 * it. */
static void
on_search_clicked(Widget w, XtPointer client_data, XtPointer call_data)
{
    LocationManagerView *view = (LocationManagerView *)client_data;
    char                 *text;
    char                  trimmed[96];
    char                 *start, *end;
    int                   n;

    (void)w;
    (void)call_data;

    text = XmTextFieldGetString(view->search_field);

    start = text;
    while (isspace((unsigned char)*start))
        start++;
    end = start + strlen(start);
    while (end > start && isspace((unsigned char)*(end - 1)))
        end--;
    *end = '\0';

    if (*start == '\0') {
        XtFree(text);
        view->search_result_count = 0;
        refresh_result_rows(view);
        return;
    }

    strncpy(trimmed, start, sizeof(trimmed) - 1);
    trimmed[sizeof(trimmed) - 1] = '\0';
    XtFree(text);

    n = weather_client_geocode_search(trimmed, view->search_results, MAX_GEOCODE_RESULTS);
    view->search_result_count = n > 0 ? n : 0;
    refresh_result_rows(view);
}

/* Fired by the "Add Location" button below the results list. Moves every
 * checked result into staged[] (silently stops once staged_count hits
 * MAX_LOCATIONS, same cap the old free-text Add enforced), then clears the
 * results list, same as Remove clears checkboxes after acting on them.
 *
 * Both name and query are set to the same full "City, Region, Country"
 * string from weather_client_geocode_format() -- not just query -- so a
 * location is always identified, end to end, by the same unique string the
 * user picked from the results list, never by a bare name that could
 * collide with some other place (e.g. "Flensburg, Schleswig-Holstein,
 * Germany" rather than a bare "Flensburg" that a differently-located
 * same-named place could also match). */
static void
on_add_location_clicked(Widget w, XtPointer client_data, XtPointer call_data)
{
    LocationManagerView *view = (LocationManagerView *)client_data;
    int                   i;
    char                  full_name[196];

    (void)w;
    (void)call_data;

    for (i = 0; i < view->search_result_count; i++) {
        if (!XmToggleButtonGetState(view->result_checks[i]))
            continue;

        if (view->staged_count >= MAX_LOCATIONS)
            break;

        weather_client_geocode_format(&view->search_results[i], full_name, sizeof(full_name));
        insert_staged_sorted(view, full_name, full_name);
    }

    view->search_result_count = 0;
    refresh_result_rows(view);
    refresh_rows(view);
}

/* Refuses to apply an empty list -- the app always needs a selected
 * location -- otherwise hands the staged entries to the controller and
 * closes, same as Cancel. */
static void
on_apply_clicked(Widget w, XtPointer client_data, XtPointer call_data)
{
    LocationManagerView *view = (LocationManagerView *)client_data;

    (void)w;
    (void)call_data;

    if (view->staged_count == 0)
        return;

    if (view->on_apply)
        view->on_apply(view->staged, view->staged_count, view->user_data);

    XtPopdown(view->shell);
}

LocationManagerView *
location_manager_view_create(Widget toplevel)
{
    LocationManagerView *view = calloc(1, sizeof(*view));
    Widget                form, scrolled_window, results_scroll;
    Atom                  wm_delete_window;
    XmString              str;

    view->shell = XtVaCreatePopupShell("manageLocationsWindow",
                                        topLevelShellWidgetClass, toplevel,
                                        XtNtitle, "XWeather - Manage Locations",
                                        XtNwidth, WINDOW_WIDTH,
                                        XtNheight, WINDOW_HEIGHT,
                                        NULL);
    set_wm_icon(view->shell);

    form = XtVaCreateManagedWidget("manageLocationsForm", xmFormWidgetClass, view->shell, NULL);

    str = XmStringCreateLocalized("Cancel");
    view->cancel_button = XtVaCreateManagedWidget("cancelButton", xmPushButtonWidgetClass, form,
                                                   XmNlabelString, str,
                                                   XmNbottomAttachment, XmATTACH_FORM,
                                                   XmNbottomOffset, 10,
                                                   XmNrightAttachment, XmATTACH_FORM,
                                                   XmNrightOffset, 10,
                                                   NULL);
    XmStringFree(str);

    str = XmStringCreateLocalized("Apply");
    view->apply_button = XtVaCreateManagedWidget("applyButton", xmPushButtonWidgetClass, form,
                                                  XmNlabelString, str,
                                                  XmNbottomAttachment, XmATTACH_FORM,
                                                  XmNbottomOffset, 10,
                                                  XmNrightAttachment, XmATTACH_WIDGET,
                                                  XmNrightWidget, view->cancel_button,
                                                  XmNrightOffset, 8,
                                                  NULL);
    XmStringFree(str);

    str = XmStringCreateLocalized("Search");
    view->search_button = XtVaCreateManagedWidget("searchButton", xmPushButtonWidgetClass, form,
                                                    XmNlabelString, str,
                                                    XmNtopAttachment, XmATTACH_FORM,
                                                    XmNtopOffset, 10,
                                                    XmNrightAttachment, XmATTACH_FORM,
                                                    XmNrightOffset, 10,
                                                    NULL);
    XmStringFree(str);

    str = XmStringCreateLocalized("Remove");
    view->remove_button = XtVaCreateManagedWidget("removeButton", xmPushButtonWidgetClass, form,
                                                   XmNlabelString, str,
                                                   XmNbottomAttachment, XmATTACH_WIDGET,
                                                   XmNbottomWidget, view->cancel_button,
                                                   XmNbottomOffset, 10,
                                                   XmNleftAttachment, XmATTACH_FORM,
                                                   XmNleftOffset, 10,
                                                   NULL);
    XmStringFree(str);

    view->search_field = XtVaCreateManagedWidget("searchField", xmTextFieldWidgetClass, form,
                                                  XmNtopAttachment, XmATTACH_FORM,
                                                  XmNtopOffset, 10,
                                                  XmNleftAttachment, XmATTACH_FORM,
                                                  XmNleftOffset, 10,
                                                  XmNrightAttachment, XmATTACH_WIDGET,
                                                  XmNrightWidget, view->search_button,
                                                  XmNrightOffset, 8,
                                                  NULL);

    /* Search results: a fixed-height list (not attached top+bottom, so it
     * doesn't compete with the staged-locations list below for the form's
     * stretchy space -- add_location_button's position is derived from its
     * bottom edge, so making this stretchy too would leave both regions'
     * heights mutually dependent on each other with no fixed point to
     * resolve from). */
    results_scroll = XtVaCreateManagedWidget("resultsScroll", xmScrolledWindowWidgetClass, form,
                                              XmNscrollingPolicy, XmAUTOMATIC,
                                              XmNscrollBarDisplayPolicy, XmSTATIC,
                                              XmNheight, RESULTS_AREA_HEIGHT,
                                              XmNtopAttachment, XmATTACH_WIDGET,
                                              XmNtopWidget, view->search_field,
                                              XmNtopOffset, 10,
                                              XmNleftAttachment, XmATTACH_FORM,
                                              XmNleftOffset, 10,
                                              XmNrightAttachment, XmATTACH_FORM,
                                              XmNrightOffset, 10,
                                              NULL);

    view->results_column = XtVaCreateManagedWidget("resultRows", xmRowColumnWidgetClass, results_scroll,
                                                     XmNorientation, XmVERTICAL,
                                                     XmNpacking, XmPACK_TIGHT,
                                                     XmNadjustLast, False,
                                                     NULL);

    unmanage_horizontal_scrollbar(results_scroll);

    str = XmStringCreateLocalized("Add Location");
    view->add_location_button = XtVaCreateManagedWidget("addLocationButton", xmPushButtonWidgetClass, form,
                                                          XmNlabelString, str,
                                                          XmNtopAttachment, XmATTACH_WIDGET,
                                                          XmNtopWidget, results_scroll,
                                                          XmNtopOffset, 10,
                                                          XmNleftAttachment, XmATTACH_FORM,
                                                          XmNleftOffset, 10,
                                                          NULL);
    XmStringFree(str);

    scrolled_window = XtVaCreateManagedWidget("locationScroll", xmScrolledWindowWidgetClass, form,
                                               XmNscrollingPolicy, XmAUTOMATIC,
                                               XmNscrollBarDisplayPolicy, XmSTATIC,
                                               XmNtopAttachment, XmATTACH_WIDGET,
                                               XmNtopWidget, view->add_location_button,
                                               XmNtopOffset, 10,
                                               XmNbottomAttachment, XmATTACH_WIDGET,
                                               XmNbottomWidget, view->remove_button,
                                               XmNbottomOffset, 10,
                                               XmNleftAttachment, XmATTACH_FORM,
                                               XmNleftOffset, 10,
                                               XmNrightAttachment, XmATTACH_FORM,
                                               XmNrightOffset, 10,
                                               NULL);

    view->row_column = XtVaCreateManagedWidget("locationRows", xmRowColumnWidgetClass, scrolled_window,
                                                XmNorientation, XmVERTICAL,
                                                XmNpacking, XmPACK_TIGHT,
                                                XmNadjustLast, False,
                                                NULL);

    unmanage_horizontal_scrollbar(scrolled_window);

    XtAddCallback(view->search_button, XmNactivateCallback, on_search_clicked, view);
    XtAddCallback(view->search_field, XmNactivateCallback, on_search_clicked, view);
    XtAddCallback(view->add_location_button, XmNactivateCallback, on_add_location_clicked, view);
    XtAddCallback(view->remove_button, XmNactivateCallback, on_remove_clicked, view);
    XtAddCallback(view->apply_button, XmNactivateCallback, on_apply_clicked, view);
    XtAddCallback(view->cancel_button, XmNactivateCallback, on_close, view->shell);

    wm_delete_window = XmInternAtom(XtDisplay(toplevel), "WM_DELETE_WINDOW", False);
    XmAddWMProtocols(view->shell, &wm_delete_window, 1);
    XmAddWMProtocolCallback(view->shell, wm_delete_window, on_close, view->shell);

    return view;
}

void
location_manager_view_destroy(LocationManagerView *view)
{
    free(view);
}

void
location_manager_view_set_apply_callback(LocationManagerView *view, LocationManagerApplyFn on_apply,
                                          void *user_data)
{
    view->on_apply  = on_apply;
    view->user_data = user_data;
}

void
location_manager_view_show(LocationManagerView *view, const LocationList *locations)
{
    int i, n = location_list_count(locations);

    view->staged_count = 0;
    for (i = 0; i < n; i++) {
        const Location *loc = location_list_get(locations, i);

        strncpy(view->staged[i].name, loc->name, sizeof(view->staged[i].name) - 1);
        view->staged[i].name[sizeof(view->staged[i].name) - 1] = '\0';
        strncpy(view->staged[i].query, loc->query, sizeof(view->staged[i].query) - 1);
        view->staged[i].query[sizeof(view->staged[i].query) - 1] = '\0';
        view->staged_count++;
    }

    XmTextFieldSetString(view->search_field, "");
    view->search_result_count = 0;
    refresh_result_rows(view);
    refresh_rows(view);

    XtPopup(view->shell, XtGrabNone);
}
