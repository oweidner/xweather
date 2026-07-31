#ifndef WEATHER_ICONS_H
#define WEATHER_ICONS_H

#include <X11/Intrinsic.h>

/* Looks up (and caches) the weather icon matching a WMO weather code,
 * composited against `context`'s own current background (see
 * weather_icons.c) so it blends into whatever widget it ends up on, under
 * any Motif color scheme. weather_icon_for_code_64() returns the
 * assets/icons/64x64 variant instead of the 32x32 one. Both return
 * XmUNSPECIFIED_PIXMAP if weather_code < 0 or the icon file is missing. */
Pixmap weather_icon_for_code(Widget context, int weather_code);
Pixmap weather_icon_for_code_64(Widget context, int weather_code);

#endif /* WEATHER_ICONS_H */
