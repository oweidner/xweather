#include <math.h>

#include "test.h"
#include "../src/locations.h"

static void
test_starts_empty(void)
{
    LocationList *list = location_list_create();

    TEST_ASSERT_INT_EQ(location_list_count(list), 0);

    location_list_destroy(list);
}

static void
test_add_sets_placeholder_state(void)
{
    LocationList    *list = location_list_create();
    const Location  *loc;

    location_list_add(list, "Aachen, North Rhine-Westphalia, Germany",
                       "Aachen, North Rhine-Westphalia, Germany");
    loc = location_list_get(list, 0);

    TEST_ASSERT_STR_EQ(loc->name, "Aachen, North Rhine-Westphalia, Germany");
    TEST_ASSERT_STR_EQ(loc->query, "Aachen, North Rhine-Westphalia, Germany");
    TEST_ASSERT_INT_EQ(loc->has_data, 0);
    TEST_ASSERT(isnan(loc->current_temperature_c));
    TEST_ASSERT_INT_EQ(loc->current_precipitation_probability, -1);
    TEST_ASSERT_INT_EQ(loc->current_is_day, 1);

    location_list_destroy(list);
}

static void
test_insertion_keeps_alphabetical_order(void)
{
    LocationList *list = location_list_create();

    /* Added out of order -- location_list_add() must sort by name as it
     * goes, not just append. */
    location_list_add(list, "Tokyo, Tokyo, Japan", "Tokyo, Tokyo, Japan");
    location_list_add(list, "Aachen, North Rhine-Westphalia, Germany",
                       "Aachen, North Rhine-Westphalia, Germany");
    location_list_add(list, "Flensburg, Schleswig-Holstein, Germany",
                       "Flensburg, Schleswig-Holstein, Germany");

    TEST_ASSERT_INT_EQ(location_list_count(list), 3);
    TEST_ASSERT_STR_EQ(location_list_get(list, 0)->name, "Aachen, North Rhine-Westphalia, Germany");
    TEST_ASSERT_STR_EQ(location_list_get(list, 1)->name, "Flensburg, Schleswig-Holstein, Germany");
    TEST_ASSERT_STR_EQ(location_list_get(list, 2)->name, "Tokyo, Tokyo, Japan");

    location_list_destroy(list);
}

static void
test_insertion_order_is_case_insensitive(void)
{
    LocationList *list = location_list_create();

    location_list_add(list, "berlin", "berlin");
    location_list_add(list, "Aachen", "Aachen");

    TEST_ASSERT_STR_EQ(location_list_get(list, 0)->name, "Aachen");
    TEST_ASSERT_STR_EQ(location_list_get(list, 1)->name, "berlin");

    location_list_destroy(list);
}

static void
test_add_stops_at_max_locations(void)
{
    LocationList *list = location_list_create();
    int           i;
    char          name[32];

    for (i = 0; i < MAX_LOCATIONS + 3; i++) {
        snprintf(name, sizeof(name), "City%02d", i);
        location_list_add(list, name, name);
    }

    /* Over-the-cap adds are silently dropped, not just left unsorted. */
    TEST_ASSERT_INT_EQ(location_list_count(list), MAX_LOCATIONS);

    location_list_destroy(list);
}

static void
test_set_data_marks_has_data(void)
{
    LocationList  *list = location_list_create();
    const Location *loc;
    DailyForecast   days[FORECAST_DAYS];
    HourlySlot      hourly[HOURLY_SLOTS];

    weather_forecast_fill_placeholder(days);
    weather_hourly_fill_placeholder(hourly);
    days[0].high_c = 21.0;
    days[0].low_c  = 12.0;

    location_list_add(list, "Aachen", "Aachen");
    TEST_ASSERT_INT_EQ(location_list_get(list, 0)->has_data, 0);

    location_list_set_data(list, 0, days, hourly, 18.5, 1, 12.0, 40);
    loc = location_list_get(list, 0);

    TEST_ASSERT_INT_EQ(loc->has_data, 1);
    TEST_ASSERT(loc->current_temperature_c == 18.5);
    TEST_ASSERT_INT_EQ(loc->current_is_day, 1);
    TEST_ASSERT(loc->current_wind_speed_kmh == 12.0);
    TEST_ASSERT_INT_EQ(loc->current_precipitation_probability, 40);
    TEST_ASSERT(loc->days[0].high_c == 21.0);

    location_list_destroy(list);
}

int
main(void)
{
    test_starts_empty();
    test_add_sets_placeholder_state();
    test_insertion_keeps_alphabetical_order();
    test_insertion_order_is_case_insensitive();
    test_add_stops_at_max_locations();
    test_set_data_marks_has_data();

    TEST_SUMMARY();
}
