#ifndef WEATHER_ICONS_H
#define WEATHER_ICONS_H

#include <X11/Intrinsic.h>

/* Looks up (and caches) the weather icon matching a WMO weather code,
 * composited against `context`'s own current background (see
 * weather_icons.c) so it blends into whatever widget it ends up on, under
 * any Motif color scheme. is_day picks the moon variant for "clear"/"few
 * clouds" when false (every other condition looks the same at night, so
 * is_day is otherwise ignored). weather_icon_for_code_24()/_64() return the
 * assets/icons/24x24 and 64x64 variants instead of the 32x32 default. All
 * return XmUNSPECIFIED_PIXMAP if weather_code < 0 or the icon file is
 * missing. */
Pixmap weather_icon_for_code(Widget context, int weather_code, int is_day);
Pixmap weather_icon_for_code_24(Widget context, int weather_code, int is_day);
Pixmap weather_icon_for_code_64(Widget context, int weather_code, int is_day);

#endif /* WEATHER_ICONS_H */
