#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /* getpid */

#include "test.h"
#include "../src/config.h"

/* Points XDG_CONFIG_HOME at a fresh scratch directory under TMPDIR (or /tmp)
 * for the current test, so config_load_locations()/config_save_locations()
 * (and the state-file equivalents) never touch the developer's real
 * ~/.config/xweather/. */
static void
use_scratch_config_home(void)
{
    static int  counter = 0;
    const char *tmpdir = getenv("TMPDIR");
    char        path[512];

    counter++;
    snprintf(path, sizeof(path), "%s/xweather-test-config-%d-%d",
             tmpdir && *tmpdir ? tmpdir : "/tmp", (int)getpid(), counter);
    setenv("XDG_CONFIG_HOME", path, 1);
}

static void
write_raw_file(const char *filename, const char *contents)
{
    char  path[512];
    char  cmd[600];
    FILE *f;

    snprintf(path, sizeof(path), "%s/xweather", getenv("XDG_CONFIG_HOME"));
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", path);
    system(cmd); /* test-only convenience; not how the app itself creates dirs */

    snprintf(path, sizeof(path), "%s/xweather/%s", getenv("XDG_CONFIG_HOME"), filename);
    f = fopen(path, "w");
    fputs(contents, f);
    fclose(f);
}

static int
file_exists(const char *filename)
{
    char  path[512];
    FILE *f;

    snprintf(path, sizeof(path), "%s/xweather/%s", getenv("XDG_CONFIG_HOME"), filename);
    f = fopen(path, "r");
    if (!f)
        return 0;
    fclose(f);
    return 1;
}

static void
test_missing_file_creates_defaults(void)
{
    AppConfig *config;

    use_scratch_config_home();

    config = config_load_locations();
    TEST_ASSERT(config_location_count(config) > 0);
    TEST_ASSERT_STR_EQ(config_location_name(config, 0), "Berlin, State of Berlin, Germany");
    TEST_ASSERT_STR_EQ(config_location_query(config, 0), "Berlin, State of Berlin, Germany");

    config_destroy(config);
}

static void
test_parses_locations_ignoring_comments_and_blanks(void)
{
    AppConfig *config;

    use_scratch_config_home();
    write_raw_file("locations",
        "# a comment line\n"
        "\n"
        "location = Aachen, North Rhine-Westphalia, Germany\n"
        "   \n"
        "# another comment\n"
        "location = Flensburg, Schleswig-Holstein, Germany\n");

    config = config_load_locations();
    TEST_ASSERT_INT_EQ(config_location_count(config), 2);
    TEST_ASSERT_STR_EQ(config_location_name(config, 0), "Aachen, North Rhine-Westphalia, Germany");
    TEST_ASSERT_STR_EQ(config_location_name(config, 1), "Flensburg, Schleswig-Holstein, Germany");

    config_destroy(config);
}

static void
test_name_and_query_are_always_the_same_value(void)
{
    AppConfig *config;

    use_scratch_config_home();
    write_raw_file("locations", "location = Aachen, North Rhine-Westphalia, Germany\n");

    config = config_load_locations();
    TEST_ASSERT_STR_EQ(config_location_name(config, 0), config_location_query(config, 0));

    config_destroy(config);
}

static void
test_save_then_load_round_trips(void)
{
    ConfigLocation entries[2];
    AppConfig     *config;

    use_scratch_config_home();

    strncpy(entries[0].name, "Aachen, North Rhine-Westphalia, Germany", sizeof(entries[0].name) - 1);
    entries[0].name[sizeof(entries[0].name) - 1] = '\0';
    strncpy(entries[0].query, entries[0].name, sizeof(entries[0].query) - 1);
    entries[0].query[sizeof(entries[0].query) - 1] = '\0';

    strncpy(entries[1].name, "Tokyo, Tokyo, Japan", sizeof(entries[1].name) - 1);
    entries[1].name[sizeof(entries[1].name) - 1] = '\0';
    strncpy(entries[1].query, entries[1].name, sizeof(entries[1].query) - 1);
    entries[1].query[sizeof(entries[1].query) - 1] = '\0';

    config_save_locations(entries, 2);

    config = config_load_locations();
    TEST_ASSERT_INT_EQ(config_location_count(config), 2);
    TEST_ASSERT_STR_EQ(config_location_name(config, 0), "Aachen, North Rhine-Westphalia, Germany");
    TEST_ASSERT_STR_EQ(config_location_name(config, 1), "Tokyo, Tokyo, Japan");

    config_destroy(config);
}

static void
test_blank_value_is_ignored(void)
{
    AppConfig *config;

    use_scratch_config_home();
    write_raw_file("locations",
        "location =    \n"
        "location = Aachen, North Rhine-Westphalia, Germany\n");

    config = config_load_locations();
    TEST_ASSERT_INT_EQ(config_location_count(config), 1);
    TEST_ASSERT_STR_EQ(config_location_name(config, 0), "Aachen, North Rhine-Westphalia, Germany");

    config_destroy(config);
}

static void
test_legacy_config_file_is_migrated(void)
{
    AppConfig *config;

    use_scratch_config_home();
    /* The old filename, before the locations/state/settings split. */
    write_raw_file("config",
        "location = Aachen, North Rhine-Westphalia, Germany\n"
        "location = Flensburg, Schleswig-Holstein, Germany\n");

    TEST_ASSERT(!file_exists("locations"));

    config = config_load_locations();
    TEST_ASSERT_INT_EQ(config_location_count(config), 2);
    TEST_ASSERT_STR_EQ(config_location_name(config, 0), "Aachen, North Rhine-Westphalia, Germany");
    TEST_ASSERT_STR_EQ(config_location_name(config, 1), "Flensburg, Schleswig-Holstein, Germany");

    /* Migrated to the new filename, and the old one left in place. */
    TEST_ASSERT(file_exists("locations"));
    TEST_ASSERT(file_exists("config"));

    config_destroy(config);
}

static void
test_active_location_round_trips(void)
{
    char out[96];

    use_scratch_config_home();

    config_save_active_location("Flensburg, Schleswig-Holstein, Germany");

    TEST_ASSERT(config_load_active_location(out, sizeof(out)));
    TEST_ASSERT_STR_EQ(out, "Flensburg, Schleswig-Holstein, Germany");
}

static void
test_active_location_missing_file_returns_not_found(void)
{
    char out[96];

    use_scratch_config_home();

    TEST_ASSERT(!config_load_active_location(out, sizeof(out)));
}

static void
test_active_location_save_overwrites_previous(void)
{
    char out[96];

    use_scratch_config_home();

    config_save_active_location("Aachen, North Rhine-Westphalia, Germany");
    config_save_active_location("Tokyo, Tokyo, Japan");

    TEST_ASSERT(config_load_active_location(out, sizeof(out)));
    TEST_ASSERT_STR_EQ(out, "Tokyo, Tokyo, Japan");
}

static void
test_active_view_round_trips(void)
{
    char out[16];

    use_scratch_config_home();

    config_save_active_view("hourly");

    TEST_ASSERT(config_load_active_view(out, sizeof(out)));
    TEST_ASSERT_STR_EQ(out, "hourly");
}

static void
test_active_view_missing_file_returns_not_found(void)
{
    char out[16];

    use_scratch_config_home();

    TEST_ASSERT(!config_load_active_view(out, sizeof(out)));
}

static void
test_active_location_and_active_view_coexist_in_state_file(void)
{
    char location_out[96];
    char view_out[16];

    /* The real bug this guards against: since both keys live in the same
     * state file, saving one must not clobber the other. */
    use_scratch_config_home();

    config_save_active_location("Flensburg, Schleswig-Holstein, Germany");
    config_save_active_view("hourly");
    config_save_active_location("Tokyo, Tokyo, Japan");

    TEST_ASSERT(config_load_active_location(location_out, sizeof(location_out)));
    TEST_ASSERT_STR_EQ(location_out, "Tokyo, Tokyo, Japan");

    TEST_ASSERT(config_load_active_view(view_out, sizeof(view_out)));
    TEST_ASSERT_STR_EQ(view_out, "hourly");
}

int
main(void)
{
    test_missing_file_creates_defaults();
    test_parses_locations_ignoring_comments_and_blanks();
    test_name_and_query_are_always_the_same_value();
    test_save_then_load_round_trips();
    test_blank_value_is_ignored();
    test_legacy_config_file_is_migrated();
    test_active_location_round_trips();
    test_active_location_missing_file_returns_not_found();
    test_active_location_save_overwrites_previous();
    test_active_view_round_trips();
    test_active_view_missing_file_returns_not_found();
    test_active_location_and_active_view_coexist_in_state_file();

    TEST_SUMMARY();
}
