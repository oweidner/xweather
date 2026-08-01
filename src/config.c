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

typedef struct {
    char name[64];
    char query[96];
} ConfigLocation;

struct AppConfig {
    ConfigLocation locations[MAX_CONFIG_LOCATIONS];
    int            location_count;
};

/* Written to a freshly created config file (and mirrored into the in-memory
 * AppConfig) when no config file exists yet at all. */
static const char *DEFAULT_LOCATION_NAMES[] = { "Berlin", "Tokyo" };
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

/* Parses a "location" value of the form "<name>[|<query>]" (see
 * docs/config.sample) and appends it to `config`. */
static void
add_location(AppConfig *config, const char *value)
{
    char        buf[160];
    char       *sep;
    const char *name, *query;
    ConfigLocation *entry;

    if (config->location_count >= MAX_CONFIG_LOCATIONS)
        return;

    strncpy(buf, value, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    sep = strchr(buf, '|');
    if (sep) {
        *sep = '\0';
        name = trim(buf);
        query = trim(sep + 1);
    } else {
        name = trim(buf);
        query = name;
    }

    if (*name == '\0')
        return;

    entry = &config->locations[config->location_count];
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    strncpy(entry->query, query, sizeof(entry->query) - 1);
    entry->query[sizeof(entry->query) - 1] = '\0';

    config->location_count++;
}

/* Parses one "key = value" line, ignoring blank lines and lines whose first
 * non-space character is '#'. `line` is modified in place. */
static void
parse_line(AppConfig *config, char *line)
{
    char *eq, *key, *value;

    line[strcspn(line, "\r\n")] = '\0';

    key = trim(line);
    if (*key == '\0' || *key == '#')
        return;

    eq = strchr(key, '=');
    if (!eq)
        return;

    *eq = '\0';
    value = trim(eq + 1);
    key = trim(key);

    if (strcmp(key, "location") == 0)
        add_location(config, value);
}

/* Fills `path` with $XDG_CONFIG_HOME/xweather/config, or
 * $HOME/.config/xweather/config if XDG_CONFIG_HOME isn't set (or is
 * empty). */
static void
resolve_config_path(char *path, size_t path_size)
{
    const char *xdg_config_home = getenv("XDG_CONFIG_HOME");
    const char *home;

    if (xdg_config_home && *xdg_config_home) {
        snprintf(path, path_size, "%s/xweather/config", xdg_config_home);
        return;
    }

    home = getenv("HOME");
    if (!home || !*home)
        home = ".";

    snprintf(path, path_size, "%s/.config/xweather/config", home);
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

/* Writes a fresh config file at `path` containing DEFAULT_LOCATION_NAMES. */
static int
write_default_config(const char *path)
{
    FILE  *f;
    size_t i;

    f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "config: failed to create \"%s\": %s\n", path, strerror(errno));
        return -1;
    }

    fprintf(f, "# xweather configuration -- see docs/config.sample for the full format\n");
    for (i = 0; i < DEFAULT_LOCATION_COUNT; i++)
        fprintf(f, "location = %s\n", DEFAULT_LOCATION_NAMES[i]);

    fclose(f);
    return 0;
}

AppConfig *
config_load(void)
{
    AppConfig *config = calloc(1, sizeof(*config));
    char       path[512];
    char       line[CONFIG_LINE_MAX];
    FILE      *f;

    resolve_config_path(path, sizeof(path));

    f = fopen(path, "r");
    if (!f) {
        char   dir[512];
        size_t i;

        fprintf(stderr, "config: no config file at \"%s\" (%s), creating one with default locations\n",
                path, strerror(errno));

        dir_of_path(path, dir, sizeof(dir));
        if (mkdir_p(dir) != 0)
            fprintf(stderr, "config: failed to create directory \"%s\": %s\n", dir, strerror(errno));
        else if (write_default_config(path) == 0)
            fprintf(stderr, "config: created \"%s\" with default locations\n", path);

        /* Whether or not writing to disk succeeded, populate the in-memory
         * config with the same defaults so this run still gets them. */
        for (i = 0; i < DEFAULT_LOCATION_COUNT; i++)
            add_location(config, DEFAULT_LOCATION_NAMES[i]);

        return config;
    }

    fprintf(stderr, "config: reading \"%s\"\n", path);

    while (fgets(line, sizeof(line), f))
        parse_line(config, line);

    fclose(f);

    fprintf(stderr, "config: loaded %d location(s) from \"%s\"\n", config->location_count, path);

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
