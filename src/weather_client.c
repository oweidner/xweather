#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <curl/curl.h>
#include <json-c/json.h>

#include "weather_client.h"

#define GEOCODING_URL_FMT "https://geocoding-api.open-meteo.com/v1/search?name=%s&count=1&language=en&format=json"
#define GEOCODING_SEARCH_URL_FMT \
    "https://geocoding-api.open-meteo.com/v1/search?name=%s&count=%d&language=en&format=json"
#define FORECAST_URL_FMT  "https://api.open-meteo.com/v1/forecast?latitude=%f&longitude=%f" \
                           "&daily=temperature_2m_max,temperature_2m_min,weathercode" \
                           "&hourly=temperature_2m,weathercode,is_day,precipitation_probability" \
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

/* Minimum length weather_client_geocode_search()'s prefix-shortening
 * fallback will shrink a query down to -- below this, a "match" is too
 * broad to be useful. */
#define MIN_FUZZY_QUERY_LEN 3

static int
geocode_search_once(const char *query, GeocodeResult *results, int max_results)
{
    CURL                *curl;
    char                *escaped_query;
    char                 url[512];
    char                *json_text;
    struct json_object  *root, *results_arr;
    int                  found = 0;
    int                  i, n;

    curl = curl_easy_init();
    if (!curl)
        return -1;

    escaped_query = curl_easy_escape(curl, query, 0);
    if (!escaped_query) {
        curl_easy_cleanup(curl);
        return -1;
    }

    snprintf(url, sizeof(url), GEOCODING_SEARCH_URL_FMT, escaped_query, max_results);
    curl_free(escaped_query);

    json_text = http_get(curl, url);
    curl_easy_cleanup(curl);
    if (!json_text)
        return -1;

    root = json_tokener_parse(json_text);
    free(json_text);
    if (!root) {
        fprintf(stderr, "weather_client: geocoding search response is not valid JSON\n");
        return -1;
    }

    if (!json_object_object_get_ex(root, "results", &results_arr) ||
        json_object_get_type(results_arr) != json_type_array) {
        json_object_put(root);
        return 0;
    }

    n = json_object_array_length(results_arr);
    if (n > max_results)
        n = max_results;

    for (i = 0; i < n; i++) {
        struct json_object *entry = json_object_array_get_idx(results_arr, i);
        struct json_object *name_obj, *admin1_obj, *country_obj;
        GeocodeResult       *out = &results[found];

        if (!json_object_object_get_ex(entry, "name", &name_obj))
            continue;

        memset(out, 0, sizeof(*out));
        strncpy(out->name, json_object_get_string(name_obj), sizeof(out->name) - 1);

        if (json_object_object_get_ex(entry, "admin1", &admin1_obj))
            strncpy(out->admin1, json_object_get_string(admin1_obj), sizeof(out->admin1) - 1);

        if (json_object_object_get_ex(entry, "country", &country_obj))
            strncpy(out->country, json_object_get_string(country_obj), sizeof(out->country) - 1);

        found++;
    }

    json_object_put(root);
    return found;
}

int
weather_client_geocode_search(const char *query, GeocodeResult *results, int max_results)
{
    int    n = geocode_search_once(query, results, max_results);
    size_t len = strlen(query);

    /* Open-Meteo's own matching is supposed to be diacritic-insensitive,
     * but isn't for every letter -- e.g. an ASCII "i" doesn't match Turkish
     * dotless "ı", so "candarli" finds nothing even though "Çandarlı" is
     * right there. Retrying with the query shortened one character at a
     * time leans on Open-Meteo's own prefix matching to route around a
     * single mismatched trailing character -- "candarli" -> "candarl"
     * still finds it. Stops at MIN_FUZZY_QUERY_LEN so a real "no such
     * place" doesn't degrade into matching almost anything, and only
     * kicks in when the query as typed found nothing at all. */
    while (n == 0 && len > MIN_FUZZY_QUERY_LEN) {
        char shortened[96];

        len--;
        if (len >= sizeof(shortened))
            continue;

        memcpy(shortened, query, len);
        shortened[len] = '\0';

        n = geocode_search_once(shortened, results, max_results);
    }

    return n;
}

void
weather_client_geocode_format(const GeocodeResult *result, char *out, size_t out_size)
{
    if (result->admin1[0] != '\0' && result->country[0] != '\0')
        snprintf(out, out_size, "%s, %s, %s", result->name, result->admin1, result->country);
    else if (result->country[0] != '\0')
        snprintf(out, out_size, "%s, %s", result->name, result->country);
    else
        snprintf(out, out_size, "%s", result->name);
}

/* Fills result->hourly[0..HOURLY_SLOTS-1] from the response's "hourly" block,
 * starting at whichever entry matches current_weather_time's date+hour (its
 * minutes don't necessarily land on :00 -- e.g. "19:30" -- so this compares
 * only the "YYYY-MM-DDTHH" prefix against hourly.time entries, which are
 * always on the hour). Falls back to the first entry if no match is found.
 * Missing entries (running off the end of the array, which shouldn't
 * normally happen since hourly covers the same forecast_days span as daily)
 * are left as NAN/empty via the placeholder fill already applied by the
 * caller. */
static void
fill_hourly_slots(struct json_object *root, const char *current_weather_time, WeatherResult *result)
{
    struct json_object *hourly, *time_arr, *temp_arr, *code_arr, *day_arr, *precip_arr;
    int                  count, start = 0;
    int                  i, j;

    if (!json_object_object_get_ex(root, "hourly", &hourly) ||
        !json_object_object_get_ex(hourly, "time", &time_arr) ||
        !json_object_object_get_ex(hourly, "temperature_2m", &temp_arr) ||
        !json_object_object_get_ex(hourly, "weathercode", &code_arr) ||
        !json_object_object_get_ex(hourly, "is_day", &day_arr)) {
        fprintf(stderr, "weather_client: forecast response missing expected hourly fields\n");
        return;
    }

    /* precipitation_probability is treated as optional -- missing it just
     * leaves current_precipitation_probability at its placeholder -1 rather
     * than failing the whole fetch. */
    precip_arr = NULL;
    json_object_object_get_ex(hourly, "precipitation_probability", &precip_arr);

    count = json_object_array_length(time_arr);

    if (current_weather_time && strlen(current_weather_time) >= 13) {
        for (i = 0; i < count; i++) {
            const char *slot_time = json_object_get_string(json_object_array_get_idx(time_arr, i));

            if (strncmp(slot_time, current_weather_time, 13) == 0) {
                start = i;
                break;
            }
        }
    }

    if (precip_arr && start < (int)json_object_array_length(precip_arr))
        result->current_precipitation_probability =
            json_object_get_int(json_object_array_get_idx(precip_arr, start));

    for (j = 0; j < HOURLY_SLOTS; j++) {
        int idx = start + j;

        if (idx >= count)
            break;

        result->hourly[j].temperature_c = json_object_get_double(json_object_array_get_idx(temp_arr, idx));
        result->hourly[j].weather_code  = json_object_get_int(json_object_array_get_idx(code_arr, idx));
        result->hourly[j].is_day        = json_object_get_int(json_object_array_get_idx(day_arr, idx));

        if (j == 0) {
            snprintf(result->hourly[j].hour_label, sizeof(result->hourly[j].hour_label), "Now");
        } else {
            const char *slot_time = json_object_get_string(json_object_array_get_idx(time_arr, idx));
            char        hh[3] = "00";

            /* ISO "YYYY-MM-DDTHH:MM" -- HH starts at offset 11. */
            if (strlen(slot_time) >= 13) {
                hh[0] = slot_time[11];
                hh[1] = slot_time[12];
            }
            snprintf(result->hourly[j].hour_label, sizeof(result->hourly[j].hour_label), "%s:00", hh);
        }
    }
}

/* Fetches the FORECAST_DAYS-day forecast and HOURLY_SLOTS-hour outlook for
 * (latitude, longitude) into result->days/result->hourly. Returns 0 on
 * success, -1 on failure. */
static int
fetch_daily_forecast(CURL *curl, double latitude, double longitude, WeatherResult *result)
{
    char                url[350];
    char               *json_text;
    struct json_object *root, *daily, *time_arr, *max_arr, *min_arr, *code_arr;
    struct json_object *current, *temp_obj, *current_time_obj, *current_is_day_obj, *windspeed_obj;
    const char         *current_weather_time = NULL;
    int                 ok = -1;
    int                 i;

    weather_hourly_fill_placeholder(result->hourly);
    result->current_is_day = 1;
    result->current_wind_speed_kmh = NAN;
    result->current_precipitation_probability = -1;

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
        if (json_object_object_get_ex(current, "time", &current_time_obj))
            current_weather_time = json_object_get_string(current_time_obj);
        if (json_object_object_get_ex(current, "is_day", &current_is_day_obj))
            result->current_is_day = json_object_get_int(current_is_day_obj);
        if (json_object_object_get_ex(current, "windspeed", &windspeed_obj))
            result->current_wind_speed_kmh = json_object_get_double(windspeed_obj);
    } else {
        fprintf(stderr, "weather_client: forecast response missing current_weather.temperature\n");
        result->current_temperature_c = NAN;
    }

    fill_hourly_slots(root, current_weather_time, result);

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
