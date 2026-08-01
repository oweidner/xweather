#ifndef HOURLY_DATA_TILE_H
#define HOURLY_DATA_TILE_H

#include <X11/Intrinsic.h>

#include "model.h"

typedef struct HourlyDataTile HourlyDataTile;

/* Creates one hourly-forecast tile as the `index`-th of `count` equal-width
 * columns in `parent`, spanning `parent`'s full height. The tile is three
 * vertically stacked cells: a fixed-height top cell (time + icon), an
 * expanding middle cell (an XmDrawingArea holding a temperature bar --
 * chosen over a plain widget since we intend to draw more into it later),
 * and a fixed-height bottom cell (temperature). Content is blank/default
 * until hourly_data_tile_set_data() is called. */
HourlyDataTile *hourly_data_tile_create(Widget parent, int index, int count);
void             hourly_data_tile_destroy(HourlyDataTile *tile);

/* Updates the tile's hour label, weather icon, temperature, and bar from
 * `slot`, and switches the whole tile's background between two shades
 * precomputed at creation time depending on `slot->is_day`. The bar's
 * height is `slot->temperature_c` normalized against [min_c, max_c] -- a
 * shared scale window computed once by the caller
 * (hourly_forecast_view_set_forecast()) so every tile's bar is drawn to the
 * same scale. */
void hourly_data_tile_set_data(HourlyDataTile *tile, const HourlySlot *slot,
                                double min_c, double max_c);

#endif /* HOURLY_DATA_TILE_H */
