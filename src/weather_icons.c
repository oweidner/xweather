#include <stdio.h>

#include <X11/Xlib.h>
#include <X11/xpm.h>
#include <Xm/Xm.h>

#include "weather_icons.h"

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

typedef enum {
    ICON_SIZE_32,
    ICON_SIZE_64,
    ICON_SIZE_COUNT
} IconSize;

/* Pre-converted from the GNOME Weather app's own "small" icon variant
 * (package gnome-weather, /usr/share/icons/hicolor/scalable/status/weather-*
 * -small.svg; GPL-2+, see /usr/share/doc/gnome-weather/copyright), with a
 * transparent ("None") background. Assumes the app is run from the project
 * root, matching how this project is built. */
static const char *icon_files[ICON_SIZE_COUNT][ICON_COUNT] = {
    [ICON_SIZE_32] = {
        "assets/icons/32x32/weather-clear.xpm",
        "assets/icons/32x32/weather-few-clouds.xpm",
        "assets/icons/32x32/weather-overcast.xpm",
        "assets/icons/32x32/weather-fog.xpm",
        "assets/icons/32x32/weather-showers.xpm",
        "assets/icons/32x32/weather-snow.xpm",
        "assets/icons/32x32/weather-storm.xpm",
    },
    [ICON_SIZE_64] = {
        "assets/icons/64x64/weather-clear.xpm",
        "assets/icons/64x64/weather-few-clouds.xpm",
        "assets/icons/64x64/weather-overcast.xpm",
        "assets/icons/64x64/weather-fog.xpm",
        "assets/icons/64x64/weather-showers.xpm",
        "assets/icons/64x64/weather-snow.xpm",
        "assets/icons/64x64/weather-storm.xpm",
    },
};

/* Icons are composited against a caller-supplied background at load time
 * (see get_icon_pixmap()), since the source files are transparent and plain
 * X Pixmaps have no per-pixel alpha of their own. That background varies by
 * context (a day card's darkened shade vs. a plain widget's inherited
 * theme color), so entries are cached per (size, kind, background) rather
 * than just per (size, kind). */
typedef struct {
    IconSize size;
    IconKind kind;
    Pixel    background;
    Pixmap   pixmap;
} IconCacheEntry;

#define MAX_ICON_CACHE_ENTRIES 32

static IconCacheEntry icon_cache[MAX_ICON_CACHE_ENTRIES];
static int            icon_cache_count = 0;

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

/* Loads icon_files[size][kind], compositing its transparent regions onto
 * `background` so it blends into whatever widget it ends up on -- rather
 * than baking in one fixed color, this queries and matches the live theme
 * at load time (see get_icon_pixmap()), so it stays correct under any Motif
 * color scheme. Returns XmUNSPECIFIED_PIXMAP if the file is missing. */
static Pixmap
composite_icon(Widget context, IconSize size, IconKind kind, Pixel background)
{
    Display     *dpy = XtDisplay(context);
    Window       root = RootWindowOfScreen(XtScreen(context));
    Pixmap       source, shape_mask, result;
    Window       geom_root;
    int          geom_x, geom_y;
    unsigned int width, height, border_width, depth;
    int          status;
    GC           gc;
    XGCValues    gcv;

    status = XpmReadFileToPixmap(dpy, root, (char *)icon_files[size][kind], &source, &shape_mask, NULL);
    if (status != XpmSuccess) {
        fprintf(stderr, "weather_icons: failed to load icon \"%s\": %s\n",
                icon_files[size][kind], XpmGetErrorString(status));
        return XmUNSPECIFIED_PIXMAP;
    }

    XGetGeometry(dpy, source, &geom_root, &geom_x, &geom_y, &width, &height, &border_width, &depth);

    result = XCreatePixmap(dpy, root, width, height, depth);

    gcv.foreground = background;
    gc = XCreateGC(dpy, result, GCForeground, &gcv);
    XFillRectangle(dpy, result, gc, 0, 0, width, height);

    if (shape_mask != None) {
        XSetClipMask(dpy, gc, shape_mask);
        XCopyArea(dpy, source, result, gc, 0, 0, width, height, 0, 0);
        XFreePixmap(dpy, shape_mask);
    } else {
        XCopyArea(dpy, source, result, gc, 0, 0, width, height, 0, 0);
    }

    XFreeGC(dpy, gc);
    XFreePixmap(dpy, source);

    return result;
}

/* Looks up (and caches) the pixmap for `kind` at `size`, composited against
 * `context`'s own current background. */
static Pixmap
get_icon_pixmap(Widget context, IconSize size, IconKind kind)
{
    Pixel  background;
    Pixmap pixmap;
    int    i;

    XtVaGetValues(context, XmNbackground, &background, NULL);

    for (i = 0; i < icon_cache_count; i++) {
        if (icon_cache[i].size == size && icon_cache[i].kind == kind &&
            icon_cache[i].background == background)
            return icon_cache[i].pixmap;
    }

    pixmap = composite_icon(context, size, kind, background);

    if (pixmap != XmUNSPECIFIED_PIXMAP && icon_cache_count < MAX_ICON_CACHE_ENTRIES) {
        icon_cache[icon_cache_count].size       = size;
        icon_cache[icon_cache_count].kind       = kind;
        icon_cache[icon_cache_count].background = background;
        icon_cache[icon_cache_count].pixmap     = pixmap;
        icon_cache_count++;
    }

    return pixmap;
}

Pixmap
weather_icon_for_code(Widget context, int weather_code)
{
    if (weather_code < 0)
        return XmUNSPECIFIED_PIXMAP;

    return get_icon_pixmap(context, ICON_SIZE_32, icon_kind_for_weather_code(weather_code));
}

Pixmap
weather_icon_for_code_64(Widget context, int weather_code)
{
    if (weather_code < 0)
        return XmUNSPECIFIED_PIXMAP;

    return get_icon_pixmap(context, ICON_SIZE_64, icon_kind_for_weather_code(weather_code));
}
