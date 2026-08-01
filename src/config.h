#ifndef CONFIG_H
#define CONFIG_H

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

#endif /* CONFIG_H */
