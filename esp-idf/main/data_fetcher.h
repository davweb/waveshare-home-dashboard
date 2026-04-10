#pragma once

#include <time.h>
#include <string.h>
#include <cJSON.h>
#include "json_tools.h"
#include "time_utils.h"

static constexpr int BUS_STOP_COUNT    = 2;
static constexpr int BUS_ARRIVAL_COUNT = 3;

struct BusArrivalData {
    char route[4];
    char destination[32];
    time_t due_epoch;
};

struct BusStopData {
    char name[32];
    BusArrivalData arrivals[BUS_ARRIVAL_COUNT];
};

struct BusData {
    BusStopData stops[BUS_STOP_COUNT];
};

struct SunData {
    bool is_sunrise;
    time_t time_utc;
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

// Parse a bus_stops JSON array (the payload of the dashboard/bus_stops MQTT topic).
inline bool parse_bus_stops(cJSON *root, BusData &bus)
{
    for (int j = 0; j < BUS_STOP_COUNT; j++) {
        cJSON *stop = cJSON_GetArrayItem(root, j);
        cJSON *buses = stop ? cJSON_GetObjectItem(stop, "buses") : nullptr;
        strlcpy(bus.stops[j].name, cjson_str(stop, "name"), sizeof(bus.stops[j].name));
        for (int i = 0; i < BUS_ARRIVAL_COUNT; i++) {
            cJSON *arrival = cJSON_GetArrayItem(buses, i);
            strlcpy(bus.stops[j].arrivals[i].route,       cjson_str(arrival, "route"),       sizeof(bus.stops[j].arrivals[i].route));
            strlcpy(bus.stops[j].arrivals[i].destination, cjson_str(arrival, "destination"), sizeof(bus.stops[j].arrivals[i].destination));
            bus.stops[j].arrivals[i].due_epoch = cjson_epoch(arrival, "due_time");
        }
    }

    return true;
}

// Parse a weather JSON object (the payload of the dashboard/weather MQTT topic).
// Populates both sun and weather out-params (sun is nested inside the weather object).
inline bool parse_weather(cJSON *root, SunData &sun, WeatherData &weather)
{
    cJSON *sun_obj = cJSON_GetObjectItem(root, "sun");
    sun.is_sunrise = cjson_bool(sun_obj, "is_sunrise");
    sun.time_utc   = cjson_epoch(sun_obj, "time_utc");

    cJSON *current = cJSON_GetObjectItem(root, "current");
    weather.current_temperature = cjson_int(current, "temperature");
    weather.current_feels_like  = cjson_int(current, "feels_like");
    strlcpy(weather.current_icon, cjson_str(current, "icon"), sizeof(weather.current_icon));

    cJSON *day = cJSON_GetObjectItem(root, "day");
    weather.day_min_temperature = cjson_int(day, "min_temperature");
    weather.day_max_temperature = cjson_int(day, "max_temperature");
    weather.day_precip_chance   = cjson_int(day, "precip_chance");
    strlcpy(weather.day_precip_type, cjson_str(day, "precip_type", "Rain"), sizeof(weather.day_precip_type));

    cJSON *hours = cJSON_GetObjectItem(root, "hours");
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

    return true;
}

// Parse a recycling JSON array (the payload of the dashboard/recycling MQTT topic).
inline bool parse_recycling(cJSON *root, RecyclingData &recycling)
{
    recycling.num_collections = cJSON_GetArraySize(root);
    if (recycling.num_collections > RECYCLING_COUNT) recycling.num_collections = RECYCLING_COUNT;

    time_t now = time(nullptr);
    for (int i = 0; i < recycling.num_collections; i++) {
        cJSON *item = cJSON_GetArrayItem(root, i);
        strlcpy(recycling.items[i].type, cjson_str(item, "type"), sizeof(recycling.items[i].type));
        recycling.items[i].date_epoch = cjson_epoch(item, "date_epoch");
        if (recycling.items[i].date_epoch > 0) {
            struct tm recycling_tm;
            localtime_r(&recycling.items[i].date_epoch, &recycling_tm);
            format_long_date(recycling.items[i].date,       sizeof(recycling.items[i].date),       &recycling_tm);
            format_short_date(recycling.items[i].short_date, sizeof(recycling.items[i].short_date), recycling.items[i].date_epoch, now);
            format_lead_time(recycling.items[i].lead_time,  sizeof(recycling.items[i].lead_time),  recycling.items[i].date_epoch, now);
        } else {
            recycling.items[i].date[0]       = '\0';
            recycling.items[i].short_date[0] = '\0';
            recycling.items[i].lead_time[0]  = '\0';
        }
    }

    return true;
}

// Parse a presence JSON array (the payload of the dashboard/presence MQTT topic).
inline bool parse_presence(cJSON *root, PresenceData &presence)
{
    presence.num_people = cJSON_GetArraySize(root);
    if (presence.num_people > PRESENCE_COUNT) presence.num_people = PRESENCE_COUNT;

    time_t now = time(nullptr);
    for (int i = 0; i < presence.num_people; i++) {
        cJSON *item = cJSON_GetArrayItem(root, i);
        strlcpy(presence.items[i].name, cjson_str(item, "name"), sizeof(presence.items[i].name));
        presence.items[i].connected = cjson_bool(item, "connected");
        if (presence.items[i].connected) {
            presence.items[i].last_seen[0] = '\0';
        } else {
            format_last_seen(presence.items[i].last_seen, sizeof(presence.items[i].last_seen),
                             cjson_epoch(item, "last_seen"), now);
        }
    }

    return true;
}
