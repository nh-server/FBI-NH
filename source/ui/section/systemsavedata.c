#include <malloc.h>
#include <stdio.h>

#include <3ds.h>

#include "action/action.h"
#include "section.h"

#define SYSTEMSAVEDATA_ACTION_COUNT 1

static u32 systemsavedata_action_count = SYSTEMSAVEDATA_ACTION_COUNT;
static list_item systemsavedata_action_items[SYSTEMSAVEDATA_ACTION_COUNT] = {
        {"Browse Save Data", 0xFF000000, action_browse_system_save_data},
};

static void systemsavedata_action_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    ui_draw_system_save_data_info(view, data, x1, y1, x2, y2);
}

static void systemsavedata_action_update(ui_view* view, void* data, list_item** items, u32** itemCount, list_item* selected, bool selectedTouched) {
    if(hidKeysDown() & KEY_B) {
        list_destroy(view);
        ui_pop();
        return;
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        void(*action)(system_save_data_info*) = (void(*)(system_save_data_info*)) selected->data;

        list_destroy(view);
        ui_pop();

        action((system_save_data_info*) data);
        return;
    }

    if(*itemCount != &systemsavedata_action_count || *items != systemsavedata_action_items) {
        *itemCount = &systemsavedata_action_count;
        *items = systemsavedata_action_items;
    }
}

static ui_view* systemsavedata_action_create(system_save_data_info* info) {
    return list_create("System Save Data Action", "A: Select, B: Return", info, systemsavedata_action_update, systemsavedata_action_draw_top);
}

static void systemsavedata_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    if(selected != NULL && selected->data != NULL) {
        ui_draw_system_save_data_info(view, selected->data, x1, y1, x2, y2);
    }
}

static void systemsavedata_update(ui_view* view, void* data, list_item** items, u32** itemCount, list_item* selected, bool selectedTouched) {
    if(hidKeysDown() & KEY_B) {
        list_destroy(view);
        ui_pop();
        return;
    }

    if(hidKeysDown() & KEY_X) {
        task_refresh_system_save_data();
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        ui_push(systemsavedata_action_create((system_save_data_info*) selected->data));
    }

    if(*itemCount != task_get_system_save_data_count() || *items != task_get_system_save_data()) {
        *itemCount = task_get_system_save_data_count();
        *items = task_get_system_save_data();
    }
}

void systemsavedata_open() {
    ui_push(list_create("System Save Data", "A: Select, B: Return, X: Refresh", NULL, systemsavedata_update, systemsavedata_draw_top));
}