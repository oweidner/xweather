#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h> /* size_t */

/* A location as stored in the locations file -- just enough to round-trip
 * it, independent of Location (locations.h), which additionally carries
 * fetched weather/fetch-state. */
typedef struct {
    char name[64];
    char query[96];
} ConfigLocation;

typedef struct AppConfig AppConfig;

/* Loads the locations file from $XDG_CONFIG_HOME/xweather/locations,
 * falling back to $HOME/.config/xweather/locations if XDG_CONFIG_HOME
 * isn't set. Always returns a usable (possibly empty) AppConfig -- if no
 * file exists or it has no location entries, config_location_count() is 0
 * and the caller is expected to fall back to its own defaults.
 *
 * If no "locations" file exists yet but a legacy ".../xweather/config"
 * file does (the old filename, same format), it's read and immediately
 * re-saved under the new "locations" filename -- a one-time, transparent
 * migration. The old file is left in place untouched. See
 * docs/locations.sample for the file format. */
AppConfig *config_load_locations(void);
void       config_destroy(AppConfig *config);

int         config_location_count(const AppConfig *config);
const char *config_location_name(const AppConfig *config, int index);
const char *config_location_query(const AppConfig *config, int index);

/* Overwrites the locations file with `entries` (one "location = name" line
 * per entry, in the same format config_load_locations() reads). Logs to
 * stderr and leaves any existing file untouched on failure -- no in-app
 * error surface for this, matching config_load_locations()'s own error
 * handling. */
void config_save_locations(const ConfigLocation *entries, int count);

/* Loads the active-location name from the state file
 * ($XDG_CONFIG_HOME/xweather/state, or ~/.config/xweather/state) into
 * `out` (out_size bytes). Returns 1 if found, 0 if the state file doesn't
 * exist yet or has no active_location entry -- caller should fall back to
 * its own default (e.g. the first location) in that case. This file is
 * app-managed, not meant for hand-editing. */
int config_load_active_location(char *out, size_t out_size);

/* Overwrites the state file's active_location entry with `name`. Creates
 * the file (and its directory) if needed. Logs to stderr and leaves any
 * existing file untouched on failure, matching config_save_locations(). */
void config_save_active_location(const char *name);

/* Loads the active-view name ("daily" or "hourly") from the same state
 * file into `out` (out_size bytes). Returns 1 if found, 0 if the state
 * file doesn't exist yet or has no active_view entry -- caller should fall
 * back to its own default (today: "daily") in that case. */
int config_load_active_view(char *out, size_t out_size);

/* Overwrites the state file's active_view entry with `name` (expected to
 * be "daily" or "hourly", though this just stores whatever string it's
 * given). Same file-creation/error handling as config_save_active_location(). */
void config_save_active_view(const char *name);

#endif /* CONFIG_H */
