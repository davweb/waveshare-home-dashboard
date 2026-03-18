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

void action_rain_chance_to_color(lv_event_t *e) {
    int chance = flow::getUserProperty(ACTION_RAIN_CHANCE_TO_COLOR_PROPERTY_RAIN_CHANCE).getInt();

    int color;
    if      (chance <  10) color = 0xDDEEFF;
    else if (chance <  20) color = 0xBBDDFF;
    else if (chance <  30) color = 0x99CCFF;
    else if (chance <  40) color = 0x77BBFF;
    else if (chance <  50) color = 0x55AAFF;
    else if (chance <  60) color = 0x3399EE;
    else if (chance <  70) color = 0x1188DD;
    else if (chance <  80) color = 0x0077CC;
    else if (chance <  90) color = 0x0055AA;
    else                   color = 0x003388;

    flow::setUserProperty(ACTION_RAIN_CHANCE_TO_COLOR_PROPERTY_COLOR, IntegerValue(color));
}
