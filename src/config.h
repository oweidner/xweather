#ifndef CONFIG_H
#define CONFIG_H

/* A location as stored in the config file -- just enough to round-trip it,
 * independent of Location (locations.h), which additionally carries
 * fetched weather/fetch-state. */
typedef struct {
    char name[64];
    char query[96];
} ConfigLocation;

typedef struct AppConfig AppConfig;

/* Loads the config file from $XDG_CONFIG_HOME/xweather/config, falling back
 * to $HOME/.config/xweather/config if XDG_CONFIG_HOME isn't set. Always
 * returns a usable (possibly empty) AppConfig -- if no file exists or it
 * has no location entries, config_location_count() is 0 and the caller is
 * expected to fall back to its own defaults. See docs/config.sample for the
 * file format. */
AppConfig *config_load(void);
void       config_destroy(AppConfig *config);

int         config_location_count(const AppConfig *config);
const char *config_location_name(const AppConfig *config, int index);
const char *config_location_query(const AppConfig *config, int index);

/* Overwrites the config file with `entries` (one "location = name" /
 * "location = name|query" line per entry, in the same format config_load()
 * reads and write_default_config() writes). Logs to stderr and leaves any
 * existing file untouched on failure -- no in-app error surface for this,
 * matching config_load()'s own error handling. */
void config_save(const ConfigLocation *entries, int count);

#endif /* CONFIG_H */
