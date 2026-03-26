#pragma once

#include <time.h>
#include <string.h>
#include <ArduinoJson.h>
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
    char event[32];
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

inline bool fetch_dashboard_data(BusData &bus, SunData &sun, WeatherData &weather, RecyclingData &recycling) {
    JsonDocument doc;

    if (!getJsonFromUrl(doc, CONFIG_DASHBOARD_SERVER_URL)) {
        return false;
    }

    // Bus stops
    for (int j = 0; j < 2; j++) {
        strlcpy(bus.stops[j].name, doc["bus_stops"][j]["name"] | "", sizeof(bus.stops[j].name));
        for (int i = 0; i < 3; i++) {
            strlcpy(bus.stops[j].arrivals[i].route, doc["bus_stops"][j]["buses"][i]["route"] | "", sizeof(bus.stops[j].arrivals[i].route));
            strlcpy(bus.stops[j].arrivals[i].destination, doc["bus_stops"][j]["buses"][i]["destination"] | "", sizeof(bus.stops[j].arrivals[i].destination));
            bus.stops[j].arrivals[i].due_epoch = doc["bus_stops"][j]["buses"][i]["due_time"] | 0L;
        }
    }

    // Sun
    strlcpy(sun.event, doc["weather"]["sun"]["event"] | "", sizeof(sun.event));
    strlcpy(sun.time, doc["weather"]["sun"]["time"] | "", sizeof(sun.time));

    // Weather current
    weather.current_temperature = doc["weather"]["current"]["temperature"] | 0;
    weather.current_feels_like  = doc["weather"]["current"]["feels_like"] | 0;
    strlcpy(weather.current_icon, doc["weather"]["current"]["icon"] | "", sizeof(weather.current_icon));

    // Weather day
    weather.day_min_temperature = doc["weather"]["day"]["min_temperature"] | 0;
    weather.day_max_temperature = doc["weather"]["day"]["max_temperature"] | 0;
    weather.day_precip_chance   = doc["weather"]["day"]["precip_chance"] | 0;
    strlcpy(weather.day_precip_type, doc["weather"]["day"]["precip_type"] | "Rain", sizeof(weather.day_precip_type));

    // Weather hours
    weather.num_hours = doc["weather"]["hours"].size();
    if (weather.num_hours > 24) weather.num_hours = 24;
    for (int i = 0; i < weather.num_hours; i++) {
        weather.hours[i].hour          = doc["weather"]["hours"][i]["hour"] | 0;
        weather.hours[i].temperature   = doc["weather"]["hours"][i]["temperature"] | 0;
        weather.hours[i].feels_like    = doc["weather"]["hours"][i]["feels_like"] | 0;
        weather.hours[i].precip_chance = doc["weather"]["hours"][i]["precip_chance"] | 0;
        weather.hours[i].wind_speed    = doc["weather"]["hours"][i]["wind_speed"] | 0;
        weather.hours[i].uv_index      = doc["weather"]["hours"][i]["uv_index"] | 0;
        strlcpy(weather.hours[i].icon, doc["weather"]["hours"][i]["icon"] | "none", sizeof(weather.hours[i].icon));
    }

    // Recycling
    recycling.num_collections = doc["recycling"].size();
    if (recycling.num_collections > RECYCLING_COUNT) recycling.num_collections = RECYCLING_COUNT;
    time_t now = time(nullptr);
    for (int i = 0; i < recycling.num_collections; i++) {
        strlcpy(recycling.items[i].type, doc["recycling"][i]["type"] | "", sizeof(recycling.items[i].type));
        recycling.items[i].date_epoch = doc["recycling"][i]["date_epoch"] | 0L;
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

    return true;
}
