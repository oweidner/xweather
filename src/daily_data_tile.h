#ifndef DAILY_DATA_TILE_H
#define DAILY_DATA_TILE_H

#include <X11/Intrinsic.h>

#include "model.h"

typedef struct DailyDataTile DailyDataTile;

/* Creates one daily-forecast tile (day name, date, weather icon, high/low)
 * as the `index`-th of `count` equal-width columns in `parent` (etched-in
 * shadow, background darkened relative to `parent`'s own). Content is
 * blank/default until daily_data_tile_set_data() is called. */
DailyDataTile *daily_data_tile_create(Widget parent, int index, int count);
void            daily_data_tile_destroy(DailyDataTile *tile);

/* Updates the tile's day name, date, weather icon, and high/low from
 * `day`. */
void daily_data_tile_set_data(DailyDataTile *tile, const DailyForecast *day);

#endif /* DAILY_DATA_TILE_H */
