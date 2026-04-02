#pragma once

#include <time.h>
#include <string.h>
#include <cJSON.h>
#include <HttpTools.h>
#include "time_utils.h"

struct BusArrivalData {
    char route[4];
    char destination[32];
    time_t due_epoch;
};

struct BusStopData {
    char name[32];
    BusArrivalData arrivals[3];
};

struct BusData {
    BusStopData stops[2];
};

struct SunData {
    bool is_sunrise;
    char time[16];
};

struct WeatherHourData {
    int hour;
    char icon[32];
    int temperature;
    int feels_like;
    int precip_chance;
    int wind_speed;
    int uv_index;
};

struct WeatherData {
    int current_temperature;
    int current_feels_like;
    char current_icon[32];

    int day_min_temperature;
    int day_max_temperature;
    int day_precip_chance;
    char day_precip_type[32];

    int num_hours;
    WeatherHourData hours[24];
};

#define PRESENCE_COUNT 5

struct PresenceItemData {
    char name[32];
    bool connected;
    char last_seen[16];
};

struct PresenceData {
    int num_people;
    PresenceItemData items[PRESENCE_COUNT];
};

#define RECYCLING_COUNT 4

struct RecyclingItemData {
    char type[32];
    time_t date_epoch;
    char date[64];
    char short_date[16];
    char lead_time[16];
};

struct RecyclingData {
    int num_collections;
    RecyclingItemData items[RECYCLING_COUNT];
};

inline bool fetch_dashboard_data(BusData &bus, SunData &sun, WeatherData &weather, RecyclingData &recycling, PresenceData &presence) {
    cJSON *root = getCJsonFromUrl(CONFIG_DASHBOARD_SERVER_URL);
    if (!root) {
        return false;
    }

    // Bus stops
    cJSON *bus_stops = cJSON_GetObjectItem(root, "bus_stops");
    for (int j = 0; j < 2; j++) {
        cJSON *stop = cJSON_GetArrayItem(bus_stops, j);
        strlcpy(bus.stops[j].name, cjson_str(stop, "name"), sizeof(bus.stops[j].name));
        cJSON *buses = stop ? cJSON_GetObjectItem(stop, "buses") : nullptr;
        for (int i = 0; i < 3; i++) {
            cJSON *arrival = cJSON_GetArrayItem(buses, i);
            strlcpy(bus.stops[j].arrivals[i].route, cjson_str(arrival, "route"), sizeof(bus.stops[j].arrivals[i].route));
            strlcpy(bus.stops[j].arrivals[i].destination, cjson_str(arrival, "destination"), sizeof(bus.stops[j].arrivals[i].destination));
            bus.stops[j].arrivals[i].due_epoch = cjson_long(arrival, "due_time");
        }
    }

    // Sun
    cJSON *weather_obj = cJSON_GetObjectItem(root, "weather");
    cJSON *sun_obj = weather_obj ? cJSON_GetObjectItem(weather_obj, "sun") : nullptr;
    sun.is_sunrise = cjson_bool(sun_obj, "is_sunrise");
    strlcpy(sun.time, cjson_str(sun_obj, "time"), sizeof(sun.time));

    // Weather current
    cJSON *current = weather_obj ? cJSON_GetObjectItem(weather_obj, "current") : nullptr;
    weather.current_temperature = cjson_int(current, "temperature");
    weather.current_feels_like  = cjson_int(current, "feels_like");
    strlcpy(weather.current_icon, cjson_str(current, "icon"), sizeof(weather.current_icon));

    // Weather day
    cJSON *day = weather_obj ? cJSON_GetObjectItem(weather_obj, "day") : nullptr;
    weather.day_min_temperature = cjson_int(day, "min_temperature");
    weather.day_max_temperature = cjson_int(day, "max_temperature");
    weather.day_precip_chance   = cjson_int(day, "precip_chance");
    strlcpy(weather.day_precip_type, cjson_str(day, "precip_type", "Rain"), sizeof(weather.day_precip_type));

    // Weather hours
    cJSON *hours = weather_obj ? cJSON_GetObjectItem(weather_obj, "hours") : nullptr;
    weather.num_hours = hours ? cJSON_GetArraySize(hours) : 0;
    if (weather.num_hours > 24) weather.num_hours = 24;
    for (int i = 0; i < weather.num_hours; i++) {
        cJSON *hour = cJSON_GetArrayItem(hours, i);
        weather.hours[i].hour          = cjson_int(hour, "hour");
        weather.hours[i].temperature   = cjson_int(hour, "temperature");
        weather.hours[i].feels_like    = cjson_int(hour, "feels_like");
        weather.hours[i].precip_chance = cjson_int(hour, "precip_chance");
        weather.hours[i].wind_speed    = cjson_int(hour, "wind_speed");
        weather.hours[i].uv_index      = cjson_int(hour, "uv_index");
        strlcpy(weather.hours[i].icon, cjson_str(hour, "icon", "none"), sizeof(weather.hours[i].icon));
    }

    // Recycling
    cJSON *recycling_arr = cJSON_GetObjectItem(root, "recycling");
    recycling.num_collections = recycling_arr ? cJSON_GetArraySize(recycling_arr) : 0;
    if (recycling.num_collections > RECYCLING_COUNT) recycling.num_collections = RECYCLING_COUNT;
    time_t now = time(nullptr);
    for (int i = 0; i < recycling.num_collections; i++) {
        cJSON *item = cJSON_GetArrayItem(recycling_arr, i);
        strlcpy(recycling.items[i].type, cjson_str(item, "type"), sizeof(recycling.items[i].type));
        recycling.items[i].date_epoch = cjson_long(item, "date_epoch");
        if (recycling.items[i].date_epoch > 0) {
            struct tm recycling_tm;
            localtime_r(&recycling.items[i].date_epoch, &recycling_tm);
            format_long_date(recycling.items[i].date, sizeof(recycling.items[i].date), &recycling_tm);
            format_short_date(recycling.items[i].short_date, sizeof(recycling.items[i].short_date), recycling.items[i].date_epoch, now);
            format_lead_time(recycling.items[i].lead_time, sizeof(recycling.items[i].lead_time), recycling.items[i].date_epoch, now);
        } else {
            recycling.items[i].date[0] = '\0';
            recycling.items[i].short_date[0] = '\0';
            recycling.items[i].lead_time[0] = '\0';
        }
    }

    // Presence
    cJSON *presence_arr = cJSON_GetObjectItem(root, "presence");
    presence.num_people = presence_arr ? cJSON_GetArraySize(presence_arr) : 0;
    if (presence.num_people > PRESENCE_COUNT) presence.num_people = PRESENCE_COUNT;
    for (int i = 0; i < presence.num_people; i++) {
        cJSON *item = cJSON_GetArrayItem(presence_arr, i);
        strlcpy(presence.items[i].name, cjson_str(item, "name"), sizeof(presence.items[i].name));
        presence.items[i].connected = cjson_bool(item, "connected");
        if (presence.items[i].connected) {
            presence.items[i].last_seen[0] = '\0';
        } else {
            format_last_seen(presence.items[i].last_seen, sizeof(presence.items[i].last_seen), cjson_long(item, "last_seen"), now);
        }
    }

    cJSON_Delete(root);
    return true;
}
