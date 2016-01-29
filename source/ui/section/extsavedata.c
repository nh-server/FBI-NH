#include <malloc.h>
#include <stdio.h>

#include <3ds.h>

#include "action/action.h"
#include "section.h"

#define EXTSAVEDATA_ACTION_COUNT 1

static u32 extsavedata_action_count = EXTSAVEDATA_ACTION_COUNT;
static list_item extsavedata_action_items[EXTSAVEDATA_ACTION_COUNT] = {
        {"Browse Save Data", 0xFF000000, action_browse_ext_save_data},
};

static void extsavedata_action_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    ui_draw_ext_save_data_info(view, data, x1, y1, x2, y2);
}

static void extsavedata_action_update(ui_view* view, void* data, list_item** items, u32** itemCount, list_item* selected, bool selectedTouched) {
    if(hidKeysDown() & KEY_B) {
        list_destroy(view);
        ui_pop();
        return;
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        void(*action)(ext_save_data_info*) = (void(*)(ext_save_data_info*)) selected->data;

        list_destroy(view);
        ui_pop();

        action((ext_save_data_info*) data);
        return;
    }

    if(*itemCount != &extsavedata_action_count || *items != extsavedata_action_items) {
        *itemCount = &extsavedata_action_count;
        *items = extsavedata_action_items;
    }
}

static ui_view* extsavedata_action_create(ext_save_data_info* info) {
    return list_create("Ext Save Data Action", "A: Select, B: Return", info, extsavedata_action_update, extsavedata_action_draw_top);
}

static void extsavedata_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    if(selected != NULL && selected->data != NULL) {
        ui_draw_ext_save_data_info(view, selected->data, x1, y1, x2, y2);
    }
}

static void extsavedata_update(ui_view* view, void* data, list_item** items, u32** itemCount, list_item* selected, bool selectedTouched) {
    if(hidKeysDown() & KEY_B) {
        list_destroy(view);
        ui_pop();
        return;
    }

    if(hidKeysDown() & KEY_X) {
        task_refresh_ext_save_data();
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        ui_push(extsavedata_action_create((ext_save_data_info*) selected->data));
    }

    if(*itemCount != task_get_ext_save_data_count() || *items != task_get_ext_save_data()) {
        *itemCount = task_get_ext_save_data_count();
        *items = task_get_ext_save_data();
    }
}

void extsavedata_open() {
    ui_push(list_create("Ext Save Data", "A: Select, B: Return, X: Refresh", NULL, extsavedata_update, extsavedata_draw_top));
}