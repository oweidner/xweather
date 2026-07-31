#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <curl/curl.h>
#include <json-c/json.h>

#include "weather_client.h"

#define GEOCODING_URL_FMT "https://geocoding-api.open-meteo.com/v1/search?name=%s&count=1&language=en&format=json"
#define FORECAST_URL_FMT  "https://api.open-meteo.com/v1/forecast?latitude=%f&longitude=%f" \
                           "&daily=temperature_2m_max,temperature_2m_min,weathercode" \
                           "&current_weather=true&timezone=auto&forecast_days=%d"

struct MemoryBuffer {
    char   *data;
    size_t  size;
};

static size_t
write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    struct MemoryBuffer *mem = (struct MemoryBuffer *)userdata;
    size_t                realsize = size * nmemb;
    char                 *newdata = realloc(mem->data, mem->size + realsize + 1);

    if (!newdata)
        return 0;

    mem->data = newdata;
    memcpy(mem->data + mem->size, ptr, realsize);
    mem->size += realsize;
    mem->data[mem->size] = '\0';

    return realsize;
}

/* Performs a GET request and returns a malloc'd response body, or NULL on
 * any transport/HTTP-status failure. Caller frees the result. */
static char *
http_get(CURL *curl, const char *url)
{
    struct MemoryBuffer mem = { NULL, 0 };
    long                 status = 0;
    CURLcode             rc;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &mem);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "xweather/1.0");

    rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
        fprintf(stderr, "weather_client: GET %s failed: %s\n", url, curl_easy_strerror(rc));
        free(mem.data);
        return NULL;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    if (status != 200) {
        fprintf(stderr, "weather_client: GET %s returned HTTP %ld\n", url, status);
        free(mem.data);
        return NULL;
    }

    return mem.data;
}

/* Turns an "YYYY-MM-DD" date into a weekday abbreviation and "Mon  D" label. */
static void
format_day_labels(const char *iso_date, DailyForecast *day)
{
    int         y = 1970, m = 1, d = 1;
    struct tm   tmv;

    sscanf(iso_date, "%d-%d-%d", &y, &m, &d);

    memset(&tmv, 0, sizeof(tmv));
    tmv.tm_year = y - 1900;
    tmv.tm_mon  = m - 1;
    tmv.tm_mday = d;
    tmv.tm_hour = 12; /* clear of any DST transition at midnight */
    mktime(&tmv);

    strftime(day->day_name, sizeof(day->day_name), "%a", &tmv);
    strftime(day->date_label, sizeof(day->date_label), "%b %e", &tmv);
}

/* Geocodes `place`, filling *latitude, *longitude and result->location.
 * Returns 0 on success, -1 on failure. */
static int
geocode(CURL *curl, const char *place, double *latitude, double *longitude,
        WeatherResult *result)
{
    char                *escaped_place;
    char                 url[512];
    char                *json_text;
    struct json_object  *root, *results, *first, *lat_obj, *lon_obj, *name_obj, *country_obj;
    int                  ok = -1;

    escaped_place = curl_easy_escape(curl, place, 0);
    if (!escaped_place)
        return -1;

    snprintf(url, sizeof(url), GEOCODING_URL_FMT, escaped_place);
    curl_free(escaped_place);

    json_text = http_get(curl, url);
    if (!json_text)
        return -1;

    root = json_tokener_parse(json_text);
    if (!root) {
        fprintf(stderr, "weather_client: geocoding response is not valid JSON: %s\n", json_text);
        free(json_text);
        return -1;
    }

    if (!json_object_object_get_ex(root, "results", &results) ||
        json_object_get_type(results) != json_type_array ||
        json_object_array_length(results) == 0) {
        fprintf(stderr, "weather_client: geocoding found no results for \"%s\": %s\n", place, json_text);
        free(json_text);
        goto done;
    }
    free(json_text);

    first = json_object_array_get_idx(results, 0);
    if (!json_object_object_get_ex(first, "latitude", &lat_obj) ||
        !json_object_object_get_ex(first, "longitude", &lon_obj) ||
        !json_object_object_get_ex(first, "name", &name_obj)) {
        fprintf(stderr, "weather_client: geocoding result missing expected fields\n");
        goto done;
    }

    *latitude  = json_object_get_double(lat_obj);
    *longitude = json_object_get_double(lon_obj);

    if (json_object_object_get_ex(first, "country", &country_obj)) {
        snprintf(result->location, sizeof(result->location), "%s, %s",
                 json_object_get_string(name_obj), json_object_get_string(country_obj));
    } else {
        snprintf(result->location, sizeof(result->location), "%s",
                 json_object_get_string(name_obj));
    }

    ok = 0;

done:
    json_object_put(root);
    return ok;
}

/* Fetches the FORECAST_DAYS-day forecast for (latitude, longitude) into
 * result->days. Returns 0 on success, -1 on failure. */
static int
fetch_daily_forecast(CURL *curl, double latitude, double longitude, WeatherResult *result)
{
    char                url[350];
    char               *json_text;
    struct json_object *root, *daily, *time_arr, *max_arr, *min_arr, *code_arr;
    struct json_object *current, *temp_obj;
    int                 ok = -1;
    int                 i;

    snprintf(url, sizeof(url), FORECAST_URL_FMT, latitude, longitude, FORECAST_DAYS);

    json_text = http_get(curl, url);
    if (!json_text)
        return -1;

    root = json_tokener_parse(json_text);
    if (!root) {
        fprintf(stderr, "weather_client: forecast response is not valid JSON: %s\n", json_text);
        free(json_text);
        return -1;
    }
    free(json_text);

    if (!json_object_object_get_ex(root, "daily", &daily) ||
        !json_object_object_get_ex(daily, "time", &time_arr) ||
        !json_object_object_get_ex(daily, "temperature_2m_max", &max_arr) ||
        !json_object_object_get_ex(daily, "temperature_2m_min", &min_arr) ||
        !json_object_object_get_ex(daily, "weathercode", &code_arr) ||
        json_object_array_length(time_arr) < FORECAST_DAYS) {
        fprintf(stderr, "weather_client: forecast response missing expected daily fields\n");
        goto done;
    }

    for (i = 0; i < FORECAST_DAYS; i++) {
        const char *iso_date = json_object_get_string(json_object_array_get_idx(time_arr, i));

        format_day_labels(iso_date, &result->days[i]);
        result->days[i].high_c       = json_object_get_double(json_object_array_get_idx(max_arr, i));
        result->days[i].low_c        = json_object_get_double(json_object_array_get_idx(min_arr, i));
        result->days[i].weather_code = json_object_get_int(json_object_array_get_idx(code_arr, i));
    }

    if (json_object_object_get_ex(root, "current_weather", &current) &&
        json_object_object_get_ex(current, "temperature", &temp_obj)) {
        result->current_temperature_c = json_object_get_double(temp_obj);
    } else {
        fprintf(stderr, "weather_client: forecast response missing current_weather.temperature\n");
        result->current_temperature_c = NAN;
    }

    ok = 0;

done:
    json_object_put(root);
    return ok;
}

int
weather_client_fetch(const char *place, WeatherResult *result)
{
    CURL  *curl;
    double latitude, longitude;
    int    ok = -1;

    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "weather_client: curl_easy_init() failed\n");
        return -1;
    }

    if (geocode(curl, place, &latitude, &longitude, result) == 0 &&
        fetch_daily_forecast(curl, latitude, longitude, result) == 0) {
        ok = 0;
    }

    curl_easy_cleanup(curl);
    return ok;
}
