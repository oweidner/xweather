#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "locations.h"

struct LocationList {
    Location entries[MAX_LOCATIONS];
    int      count;
};

LocationList *
location_list_create(void)
{
    return calloc(1, sizeof(LocationList));
}

void
location_list_destroy(LocationList *list)
{
    free(list);
}

/* Keeps entries sorted alphabetically (case-insensitive) as they're added,
 * so callers never need to sort the list themselves. */
void
location_list_add(LocationList *list, const char *name, const char *query)
{
    Location *entry;
    int       insert_at;

    if (list->count >= MAX_LOCATIONS)
        return;

    for (insert_at = 0; insert_at < list->count; insert_at++) {
        if (strcasecmp(name, list->entries[insert_at].name) < 0)
            break;
    }

    memmove(&list->entries[insert_at + 1], &list->entries[insert_at],
            (size_t)(list->count - insert_at) * sizeof(Location));

    entry = &list->entries[insert_at];
    memset(entry, 0, sizeof(*entry));
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    strncpy(entry->query, query, sizeof(entry->query) - 1);
    entry->query[sizeof(entry->query) - 1] = '\0';

    /* Pending fetch: show N/A everywhere, not a zeroed 0degC/clear-sky icon.
     * current_temperature_c must be NAN (not 0.0) since the async controller
     * now displays this entry's data immediately, before any fetch completes. */
    weather_forecast_fill_placeholder(entry->days);
    entry->current_temperature_c = NAN;

    list->count++;
}

int
location_list_count(const LocationList *list)
{
    return list->count;
}

const Location *
location_list_get(const LocationList *list, int index)
{
    return &list->entries[index];
}

void
location_list_set_data(LocationList *list, int index, const DailyForecast *days,
                        double current_temperature_c)
{
    Location *entry = &list->entries[index];

    memcpy(entry->days, days, sizeof(entry->days));
    entry->current_temperature_c = current_temperature_c;
    entry->has_data = 1;
}
