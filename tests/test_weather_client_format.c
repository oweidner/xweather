#include <string.h>

#include "test.h"
#include "../src/weather_client.h"

static void
test_full_result_formats_name_admin1_country(void)
{
    GeocodeResult result;
    char          out[196];

    memset(&result, 0, sizeof(result));
    strcpy(result.name, "Flensburg");
    strcpy(result.admin1, "Schleswig-Holstein");
    strcpy(result.country, "Germany");

    weather_client_geocode_format(&result, out, sizeof(out));
    TEST_ASSERT_STR_EQ(out, "Flensburg, Schleswig-Holstein, Germany");
}

static void
test_missing_admin1_falls_back_to_name_and_country(void)
{
    GeocodeResult result;
    char          out[196];

    memset(&result, 0, sizeof(result));
    strcpy(result.name, "Tokyo");
    strcpy(result.country, "Japan");

    weather_client_geocode_format(&result, out, sizeof(out));
    TEST_ASSERT_STR_EQ(out, "Tokyo, Japan");
}

static void
test_missing_admin1_and_country_falls_back_to_name_only(void)
{
    GeocodeResult result;
    char          out[196];

    memset(&result, 0, sizeof(result));
    strcpy(result.name, "Somewhere");

    weather_client_geocode_format(&result, out, sizeof(out));
    TEST_ASSERT_STR_EQ(out, "Somewhere");
}

static void
test_country_without_admin1_is_used(void)
{
    GeocodeResult result;
    char          out[196];

    /* admin1 empty, country present -- must not silently drop the
     * country just because admin1 is missing. */
    memset(&result, 0, sizeof(result));
    strcpy(result.name, "Reykjavik");
    strcpy(result.country, "Iceland");

    weather_client_geocode_format(&result, out, sizeof(out));
    TEST_ASSERT_STR_EQ(out, "Reykjavik, Iceland");
}

int
main(void)
{
    test_full_result_formats_name_admin1_country();
    test_missing_admin1_falls_back_to_name_and_country();
    test_missing_admin1_and_country_falls_back_to_name_only();
    test_country_without_admin1_is_used();

    TEST_SUMMARY();
}
