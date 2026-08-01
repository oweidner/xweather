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

#include "location_manager_view.h"

#define WINDOW_WIDTH  380
#define WINDOW_HEIGHT 420

/* Same icon file as the main window (see view.c's set_wm_icon) -- duplicated
 * rather than shared, same as this codebase's existing BOLD_FONT/
 * create_bold_render_table duplication between view.c and daily_data_tile.c. */
#define APP_ICON_FILE "assets/icons/64x64/weather-clear.xpm"

struct LocationManagerView {
    Widget shell;
    Widget search_field;
    Widget add_button;
    Widget row_column;
    Widget apply_button;
    Widget cancel_button;

    /* The in-window copy being edited -- Add/remove only touch this, never
     * anything outside the window. Reset from the live LocationList every
     * time the window is shown; only Apply pushes it back out. */
    ConfigLocation staged[MAX_LOCATIONS];
    int            staged_count;

    /* Per-row widgets, parallel to staged[0..rows_built-1], so refresh_rows()
     * knows what to destroy and on_row_remove() can identify which row fired
     * by scanning remove_buttons[] for the widget that was clicked -- same
     * pattern view.c's location_items[]/on_location_selected() uses. */
    Widget row_forms[MAX_LOCATIONS];
    Widget remove_buttons[MAX_LOCATIONS];
    int    rows_built;

    LocationManagerApplyFn on_apply;
    void                   *user_data;
};

static void refresh_rows(LocationManagerView *view);

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

static void
on_row_remove(Widget w, XtPointer client_data, XtPointer call_data)
{
    LocationManagerView *view = (LocationManagerView *)client_data;
    int                   i;

    (void)call_data;

    for (i = 0; i < view->staged_count; i++) {
        if (view->remove_buttons[i] != w)
            continue;

        memmove(&view->staged[i], &view->staged[i + 1],
                (size_t)(view->staged_count - i - 1) * sizeof(view->staged[0]));
        view->staged_count--;
        refresh_rows(view);
        return;
    }
}

/* Destroys and rebuilds every row from view->staged[], and updates the Add
 * button's sensitivity for the new count. Called after every staged-list
 * change (add, remove, or a fresh location_manager_view_show()). */
static void
refresh_rows(LocationManagerView *view)
{
    int i;

    for (i = 0; i < view->rows_built; i++)
        XtDestroyWidget(view->row_forms[i]); /* also destroys the row's label + remove button */

    for (i = 0; i < view->staged_count; i++) {
        Widget   row, remove_btn;
        XmString str;

        row = XtVaCreateManagedWidget("locationRow", xmFormWidgetClass, view->row_column, NULL);

        str = XmStringCreateLocalized(view->staged[i].name);
        XtVaCreateManagedWidget("locationName", xmLabelWidgetClass, row,
                                 XmNlabelString, str,
                                 XmNalignment, XmALIGNMENT_BEGINNING,
                                 XmNtopAttachment, XmATTACH_FORM,
                                 XmNbottomAttachment, XmATTACH_FORM,
                                 XmNleftAttachment, XmATTACH_FORM,
                                 XmNleftOffset, 6,
                                 NULL);
        XmStringFree(str);

        str = XmStringCreateLocalized("X");
        remove_btn = XtVaCreateManagedWidget("locationRemove", xmPushButtonWidgetClass, row,
                                              XmNlabelString, str,
                                              XmNtopAttachment, XmATTACH_FORM,
                                              XmNbottomAttachment, XmATTACH_FORM,
                                              XmNrightAttachment, XmATTACH_FORM,
                                              XmNrightOffset, 6,
                                              NULL);
        XmStringFree(str);
        XtAddCallback(remove_btn, XmNactivateCallback, on_row_remove, view);

        view->row_forms[i]      = row;
        view->remove_buttons[i] = remove_btn;
    }

    view->rows_built = view->staged_count;

    XtSetSensitive(view->add_button, view->staged_count < MAX_LOCATIONS);
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

/* Fired by both the Add button and the search field's Enter key. Reads and
 * trims the search field, ignores it if empty or the list is already at
 * MAX_LOCATIONS, otherwise stages it (name == query, same convention as
 * every existing default location -- no pre-validation geocode, the normal
 * fetch/retry/error-status machinery handles a bad query once applied,
 * same as it already does for any existing location today). */
static void
on_add_clicked(Widget w, XtPointer client_data, XtPointer call_data)
{
    LocationManagerView *view = (LocationManagerView *)client_data;
    char                 *text;
    char                  trimmed[96];
    char                 *start, *end;

    (void)w;
    (void)call_data;

    if (view->staged_count >= MAX_LOCATIONS)
        return;

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
        return;
    }

    strncpy(trimmed, start, sizeof(trimmed) - 1);
    trimmed[sizeof(trimmed) - 1] = '\0';
    XtFree(text);

    insert_staged_sorted(view, trimmed, trimmed);
    XmTextFieldSetString(view->search_field, "");
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
    Widget                form, scrolled_window;
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

    str = XmStringCreateLocalized("Add");
    view->add_button = XtVaCreateManagedWidget("addButton", xmPushButtonWidgetClass, form,
                                                XmNlabelString, str,
                                                XmNtopAttachment, XmATTACH_FORM,
                                                XmNtopOffset, 10,
                                                XmNrightAttachment, XmATTACH_FORM,
                                                XmNrightOffset, 10,
                                                NULL);
    XmStringFree(str);

    view->search_field = XtVaCreateManagedWidget("searchField", xmTextFieldWidgetClass, form,
                                                  XmNtopAttachment, XmATTACH_FORM,
                                                  XmNtopOffset, 10,
                                                  XmNleftAttachment, XmATTACH_FORM,
                                                  XmNleftOffset, 10,
                                                  XmNrightAttachment, XmATTACH_WIDGET,
                                                  XmNrightWidget, view->add_button,
                                                  XmNrightOffset, 8,
                                                  NULL);

    scrolled_window = XtVaCreateManagedWidget("locationScroll", xmScrolledWindowWidgetClass, form,
                                               XmNscrollingPolicy, XmAUTOMATIC,
                                               XmNtopAttachment, XmATTACH_WIDGET,
                                               XmNtopWidget, view->search_field,
                                               XmNtopOffset, 10,
                                               XmNbottomAttachment, XmATTACH_WIDGET,
                                               XmNbottomWidget, view->cancel_button,
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

    XtAddCallback(view->add_button, XmNactivateCallback, on_add_clicked, view);
    XtAddCallback(view->search_field, XmNactivateCallback, on_add_clicked, view);
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
    refresh_rows(view);

    XtPopup(view->shell, XtGrabNone);
}
