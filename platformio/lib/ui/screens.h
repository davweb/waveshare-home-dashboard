#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Screens

enum ScreensEnum {
    _SCREEN_ID_FIRST = 1,
    SCREEN_ID_MAIN = 1,
    _SCREEN_ID_LAST = 1
};

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *top;
    lv_obj_t *top__box;
    lv_obj_t *top__row1;
    lv_obj_t *top__row1__bus_route;
    lv_obj_t *top__row1__bus_destination;
    lv_obj_t *top__row1__bus_due;
    lv_obj_t *top__row2;
    lv_obj_t *top__row2__bus_route;
    lv_obj_t *top__row2__bus_destination;
    lv_obj_t *top__row2__bus_due;
    lv_obj_t *top__row3;
    lv_obj_t *top__row3__bus_route;
    lv_obj_t *top__row3__bus_destination;
    lv_obj_t *top__row3__bus_due;
    lv_obj_t *top__stop_name;
    lv_obj_t *bottom;
    lv_obj_t *bottom__box;
    lv_obj_t *bottom__row1;
    lv_obj_t *bottom__row1__bus_route;
    lv_obj_t *bottom__row1__bus_destination;
    lv_obj_t *bottom__row1__bus_due;
    lv_obj_t *bottom__row2;
    lv_obj_t *bottom__row2__bus_route;
    lv_obj_t *bottom__row2__bus_destination;
    lv_obj_t *bottom__row2__bus_due;
    lv_obj_t *bottom__row3;
    lv_obj_t *bottom__row3__bus_route;
    lv_obj_t *bottom__row3__bus_destination;
    lv_obj_t *bottom__row3__bus_due;
    lv_obj_t *bottom__stop_name;
} objects_t;

extern objects_t objects;

void create_screen_main();
void tick_screen_main();

void create_user_widget_bus_time(lv_obj_t *parent_obj, int startWidgetIndex);
void tick_user_widget_bus_time(int startWidgetIndex);

void create_user_widget_bus_times(lv_obj_t *parent_obj, int startWidgetIndex);
void tick_user_widget_bus_times(int startWidgetIndex);

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/