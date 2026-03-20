#pragma once

#include <lvgl.h>
#include <ui.h>
#include <actions.h>


void action_temperature_to_color(lv_event_t *e) {
    int temp = flow::getUserProperty(ACTION_TEMPERATURE_TO_COLOR_PROPERTY_TEMPERATURE).getInt();

    int color;
    if      (temp <  0) color = 0x7EC8E3;
    else if (temp <  5) color = 0x6B9FE4;
    else if (temp < 10) color = 0x3DBFB8;
    else if (temp < 15) color = 0x5DD96B;
    else if (temp < 20) color = 0xC8D44A;
    else if (temp < 25) color = 0xF5A623;
    else if (temp < 30) color = 0xF46B1A;
    else                color = 0xFF3B3B;

    flow::setUserProperty(ACTION_TEMPERATURE_TO_COLOR_PROPERTY_COLOR, IntegerValue(color));
}

void action_reset_weather_scroll(lv_event_t *e) {
    lv_obj_scroll_to_x(objects.weather_hours, 0, LV_ANIM_OFF);
}

void action_uv_index_to_color(lv_event_t *e) {
    int uv = flow::getUserProperty(ACTION_UV_INDEX_TO_COLOR_PROPERTY_UV_INDEX).getInt();

    int color;
    if      (uv <= 1)  color = 0x4CAF50; // green
    else if (uv == 2)  color = 0x8BC34A; // yellow-green
    else if (uv == 3)  color = 0xFFEB3B; // yellow
    else if (uv == 4)  color = 0xFFC107; // yellow-orange
    else if (uv == 5)  color = 0xFF9800; // orange
    else if (uv == 6)  color = 0xFF6F00; // dark orange
    else if (uv == 7)  color = 0xFF5722; // orange-red
    else if (uv == 8)  color = 0xF44336; // red
    else if (uv == 9)  color = 0xE91E63; // hot pink
    else if (uv == 10) color = 0xAB47BC; // purple-pink
    else               color = 0x9575CD; // purple (11+)

    flow::setUserProperty(ACTION_UV_INDEX_TO_COLOR_PROPERTY_COLOR, IntegerValue(color));
}

void action_rain_chance_to_color(lv_event_t *e) {
    int chance = flow::getUserProperty(ACTION_RAIN_CHANCE_TO_COLOR_PROPERTY_RAIN_CHANCE).getInt();

    int color;
    if      (chance <  10) color = 0x99CCFF;
    else if (chance <  20) color = 0x88BBEE;
    else if (chance <  30) color = 0x77AADD;
    else if (chance <  40) color = 0x6699CC;
    else if (chance <  50) color = 0x5588BB;
    else if (chance <  60) color = 0x4477AA;
    else if (chance <  70) color = 0x336699;
    else if (chance <  80) color = 0x225588;
    else if (chance <  90) color = 0x114499;
    else                   color = 0x0055AA;

    flow::setUserProperty(ACTION_RAIN_CHANCE_TO_COLOR_PROPERTY_COLOR, IntegerValue(color));
}
