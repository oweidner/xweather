#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "config.h"

#define MAX_CONFIG_LOCATIONS 32
#define CONFIG_LINE_MAX      256

struct AppConfig {
    ConfigLocation locations[MAX_CONFIG_LOCATIONS];
    int            location_count;
};

/* Written to a freshly created locations file (and mirrored into the
 * in-memory AppConfig) when no locations file -- and no legacy config
 * file to migrate -- exists yet at all. Each is the full "City, Region,
 * Country" string Open-Meteo's geocoding search itself returns for that
 * city (verified by hand against the live API), the same convention the
 * Manage Locations search now uses for every location it adds -- see
 * weather_client_geocode_format() -- so a default resolves to exactly one
 * place, never whatever the geocoder happens to rank first for a bare,
 * ambiguous name. */
static const char *DEFAULT_LOCATION_NAMES[] = {
    "Berlin, State of Berlin, Germany",
    "Tokyo, Tokyo, Japan",
};
#define DEFAULT_LOCATION_COUNT \
    (sizeof(DEFAULT_LOCATION_NAMES) / sizeof(DEFAULT_LOCATION_NAMES[0]))

/* Trims leading/trailing whitespace in place and returns a pointer to the
 * first non-space character (the buffer itself is NUL-shortened, not
 * copied). */
static char *
trim(char *s)
{
    char *end;

    while (isspace((unsigned char)*s))
        s++;

    if (*s == '\0')
        return s;

    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end))
        end--;
    end[1] = '\0';

    return s;
}

/* Splits one "key = value" line in place, trimming both sides. Returns 0
 * (and leaves the outputs unset) for a blank line, a '#'-comment, or a
 * line with no '='; callers should just skip those. Shared by the
 * locations-file and state-file parsers. `line` is modified in place. */
static int
split_key_value(char *line, char **key, char **value)
{
    char *eq;

    line[strcspn(line, "\r\n")] = '\0';

    *key = trim(line);
    if (**key == '\0' || **key == '#')
        return 0;

    eq = strchr(*key, '=');
    if (!eq)
        return 0;

    *eq = '\0';
    *value = trim(eq + 1);
    *key = trim(*key);

    return 1;
}

/* Parses a "location" value -- a single "City, Region, Country" string (see
 * docs/locations.sample) -- and appends it to `config`, using it as both
 * name and query. */
static void
add_location(AppConfig *config, const char *value)
{
    char        buf[160];
    const char *name;
    ConfigLocation *entry;

    if (config->location_count >= MAX_CONFIG_LOCATIONS)
        return;

    strncpy(buf, value, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    name = trim(buf);
    if (*name == '\0')
        return;

    entry = &config->locations[config->location_count];
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    strncpy(entry->query, name, sizeof(entry->query) - 1);
    entry->query[sizeof(entry->query) - 1] = '\0';

    config->location_count++;
}

/* Parses one "key = value" line of the locations file. */
static void
parse_locations_line(AppConfig *config, char *line)
{
    char *key, *value;

    if (!split_key_value(line, &key, &value))
        return;

    if (strcmp(key, "location") == 0)
        add_location(config, value);
}

/* Fills `path` with $XDG_CONFIG_HOME/xweather/<filename>, or
 * $HOME/.config/xweather/<filename> if XDG_CONFIG_HOME isn't set (or is
 * empty). Shared by every file this module manages (locations, state, and
 * -- eventually -- settings), so they all live in the same directory and
 * resolve the same way. */
static void
resolve_config_path(const char *filename, char *path, size_t path_size)
{
    const char *xdg_config_home = getenv("XDG_CONFIG_HOME");
    const char *home;

    if (xdg_config_home && *xdg_config_home) {
        snprintf(path, path_size, "%s/xweather/%s", xdg_config_home, filename);
        return;
    }

    home = getenv("HOME");
    if (!home || !*home)
        home = ".";

    snprintf(path, path_size, "%s/.config/xweather/%s", home, filename);
}

/* Fills `dir` with the directory portion of `path` (everything before the
 * last '/'), or "." if `path` has no '/'. */
static void
dir_of_path(const char *path, char *dir, size_t dir_size)
{
    const char *slash = strrchr(path, '/');
    size_t      len;

    if (!slash) {
        snprintf(dir, dir_size, ".");
        return;
    }

    len = (size_t)(slash - path);
    if (len >= dir_size)
        len = dir_size - 1;
    memcpy(dir, path, len);
    dir[len] = '\0';
}

/* mkdir -p: creates `dir` and any missing parent directories. */
static int
mkdir_p(const char *dir)
{
    char   tmp[512];
    char  *p;
    size_t len = strlen(dir);

    if (len == 0 || len >= sizeof(tmp))
        return -1;

    strcpy(tmp, dir);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = '\0';

    for (p = tmp + 1; *p; p++) {
        if (*p != '/')
            continue;
        *p = '\0';
        if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
            return -1;
        *p = '/';
    }

    if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
        return -1;

    return 0;
}

/* Writes a fresh locations file at `path` containing DEFAULT_LOCATION_NAMES. */
static int
write_default_locations(const char *path)
{
    FILE  *f;
    size_t i;

    f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "config: failed to create \"%s\": %s\n", path, strerror(errno));
        return -1;
    }

    fprintf(f, "# xweather locations -- see docs/locations.sample for the full format\n");
    for (i = 0; i < DEFAULT_LOCATION_COUNT; i++)
        fprintf(f, "location = %s\n", DEFAULT_LOCATION_NAMES[i]);

    fclose(f);
    return 0;
}

void
config_save_locations(const ConfigLocation *entries, int count)
{
    char  path[512];
    char  dir[512];
    int   i;
    FILE *f;

    resolve_config_path("locations", path, sizeof(path));

    dir_of_path(path, dir, sizeof(dir));
    if (mkdir_p(dir) != 0) {
        fprintf(stderr, "config: failed to create directory \"%s\": %s\n", dir, strerror(errno));
        return;
    }

    f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "config: failed to write \"%s\": %s\n", path, strerror(errno));
        return;
    }

    fprintf(f, "# xweather locations -- see docs/locations.sample for the full format\n");
    for (i = 0; i < count; i++)
        fprintf(f, "location = %s\n", entries[i].name);

    fclose(f);
    fprintf(stderr, "config: saved %d location(s) to \"%s\"\n", count, path);
}

/* Reads and parses the locations file at `path` (already resolved) into
 * `config`. Returns 1 if the file existed and was read (even if it had no
 * usable location lines), 0 if it didn't exist at all. Shared by the real
 * load and by the legacy-file migration check in config_load_locations(). */
static int
load_locations_from_path(AppConfig *config, const char *path)
{
    char  line[CONFIG_LINE_MAX];
    FILE *f = fopen(path, "r");

    if (!f)
        return 0;

    while (fgets(line, sizeof(line), f))
        parse_locations_line(config, line);

    fclose(f);
    return 1;
}

AppConfig *
config_load_locations(void)
{
    AppConfig *config = calloc(1, sizeof(*config));
    char       path[512];
    char       legacy_path[512];

    resolve_config_path("locations", path, sizeof(path));

    if (load_locations_from_path(config, path)) {
        fprintf(stderr, "config: loaded %d location(s) from \"%s\"\n", config->location_count, path);
        return config;
    }

    /* No "locations" file yet -- check for the old "config" filename in
     * the same directory (identical format) and migrate it transparently
     * if found, so a rename of this file doesn't lose anyone's saved
     * locations. The legacy file is left in place, untouched. */
    resolve_config_path("config", legacy_path, sizeof(legacy_path));
    if (load_locations_from_path(config, legacy_path)) {
        fprintf(stderr, "config: migrating legacy \"%s\" to \"%s\"\n", legacy_path, path);
        config_save_locations(config->locations, config->location_count);
        return config;
    }

    {
        char   dir[512];
        size_t i;

        fprintf(stderr, "config: no locations file at \"%s\", creating one with default locations\n", path);

        dir_of_path(path, dir, sizeof(dir));
        if (mkdir_p(dir) != 0)
            fprintf(stderr, "config: failed to create directory \"%s\": %s\n", dir, strerror(errno));
        else if (write_default_locations(path) == 0)
            fprintf(stderr, "config: created \"%s\" with default locations\n", path);

        /* Whether or not writing to disk succeeded, populate the in-memory
         * config with the same defaults so this run still gets them. */
        for (i = 0; i < DEFAULT_LOCATION_COUNT; i++)
            add_location(config, DEFAULT_LOCATION_NAMES[i]);
    }

    return config;
}

void
config_destroy(AppConfig *config)
{
    free(config);
}

int
config_location_count(const AppConfig *config)
{
    return config->location_count;
}

const char *
config_location_name(const AppConfig *config, int index)
{
    return config->locations[index].name;
}

const char *
config_location_query(const AppConfig *config, int index)
{
    return config->locations[index].query;
}

/* The state file holds more than one key (active_location, active_view, ...
 * more to come), so saving one key has to preserve whatever else is
 * already in the file rather than blindly overwriting it -- unlike the
 * locations file, which is always rewritten wholesale from a complete list
 * the caller already has in hand. */
#define MAX_STATE_ENTRIES 8

typedef struct {
    char key[32];
    char value[96];
} StateEntry;

/* Reads every "key = value" line from the state file into `entries` (up to
 * max_entries), returning how many were found (0 if the file doesn't exist
 * or has none). */
static int
load_state_entries(StateEntry *entries, int max_entries)
{
    char  path[512];
    char  line[CONFIG_LINE_MAX];
    FILE *f;
    int   count = 0;

    resolve_config_path("state", path, sizeof(path));
    f = fopen(path, "r");
    if (!f)
        return 0;

    while (count < max_entries && fgets(line, sizeof(line), f)) {
        char *key, *value;

        if (!split_key_value(line, &key, &value))
            continue;

        strncpy(entries[count].key, key, sizeof(entries[count].key) - 1);
        entries[count].key[sizeof(entries[count].key) - 1] = '\0';
        strncpy(entries[count].value, value, sizeof(entries[count].value) - 1);
        entries[count].value[sizeof(entries[count].value) - 1] = '\0';
        count++;
    }

    fclose(f);
    return count;
}

/* Overwrites the state file with exactly `entries`, one "key = value" line
 * each. */
static void
save_state_entries(const StateEntry *entries, int count)
{
    char  path[512];
    char  dir[512];
    FILE *f;
    int   i;

    resolve_config_path("state", path, sizeof(path));

    dir_of_path(path, dir, sizeof(dir));
    if (mkdir_p(dir) != 0) {
        fprintf(stderr, "config: failed to create directory \"%s\": %s\n", dir, strerror(errno));
        return;
    }

    f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "config: failed to write \"%s\": %s\n", path, strerror(errno));
        return;
    }

    for (i = 0; i < count; i++)
        fprintf(f, "%s = %s\n", entries[i].key, entries[i].value);

    fclose(f);
}

/* Sets `key`'s value in the state file to `value`, preserving every other
 * key already there (read-modify-write); appends it if the key isn't
 * present yet, up to MAX_STATE_ENTRIES. */
static void
set_state_entry(const char *key, const char *value)
{
    StateEntry entries[MAX_STATE_ENTRIES];
    int        count = load_state_entries(entries, MAX_STATE_ENTRIES);
    int        i;

    for (i = 0; i < count; i++) {
        if (strcmp(entries[i].key, key) == 0) {
            strncpy(entries[i].value, value, sizeof(entries[i].value) - 1);
            entries[i].value[sizeof(entries[i].value) - 1] = '\0';
            save_state_entries(entries, count);
            return;
        }
    }

    if (count < MAX_STATE_ENTRIES) {
        strncpy(entries[count].key, key, sizeof(entries[count].key) - 1);
        entries[count].key[sizeof(entries[count].key) - 1] = '\0';
        strncpy(entries[count].value, value, sizeof(entries[count].value) - 1);
        entries[count].value[sizeof(entries[count].value) - 1] = '\0';
        count++;
    }

    save_state_entries(entries, count);
}

/* Reads `key`'s value from the state file into `out` (out_size bytes).
 * Returns 1 if found and non-empty, 0 otherwise. */
static int
get_state_entry(const char *key, char *out, size_t out_size)
{
    StateEntry entries[MAX_STATE_ENTRIES];
    int        count = load_state_entries(entries, MAX_STATE_ENTRIES);
    int        i;

    out[0] = '\0';

    for (i = 0; i < count; i++) {
        if (strcmp(entries[i].key, key) == 0) {
            strncpy(out, entries[i].value, out_size - 1);
            out[out_size - 1] = '\0';
            return out[0] != '\0';
        }
    }

    return 0;
}

int
config_load_active_location(char *out, size_t out_size)
{
    return get_state_entry("active_location", out, out_size);
}

void
config_save_active_location(const char *name)
{
    set_state_entry("active_location", name);
}

int
config_load_active_view(char *out, size_t out_size)
{
    return get_state_entry("active_view", out, out_size);
}

void
config_save_active_view(const char *name)
{
    set_state_entry("active_view", name);
}
