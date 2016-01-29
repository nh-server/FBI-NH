#include <malloc.h>
#include <stdio.h>

#include <3ds.h>

#include "action/action.h"
#include "section.h"

#define TITLES_ACTION_COUNT 5

static u32 titles_action_count = TITLES_ACTION_COUNT;
static list_item titles_action_items[TITLES_ACTION_COUNT] = {
        {"Launch Title", 0xFF000000, action_launch_title},
        {"Delete Title", 0xFF000000, action_delete_title},
        {"Browse Save Data", 0xFF000000, action_browse_title_save_data},
        {"Import Secure Value", 0xFF000000, action_import_secure_value},
        {"Export Secure Value", 0xFF000000, action_export_secure_value},
};

static void titles_action_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    ui_draw_title_info(view, data, x1, y1, x2, y2);
}

static void titles_action_update(ui_view* view, void* data, list_item** items, u32** itemCount, list_item* selected, bool selectedTouched) {
    if(hidKeysDown() & KEY_B) {
        list_destroy(view);
        ui_pop();
        return;
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        void(*action)(title_info*) = (void(*)(title_info*)) selected->data;

        list_destroy(view);
        ui_pop();

        action((title_info*) data);
        return;
    }

    if(*itemCount != &titles_action_count || *items != titles_action_items) {
        *itemCount = &titles_action_count;
        *items = titles_action_items;
    }
}

static ui_view* titles_action_create(title_info* info) {
    return list_create("Title Action", "A: Select, B: Return", info, titles_action_update, titles_action_draw_top);
}

static void titles_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    if(selected != NULL && selected->data != NULL) {
        ui_draw_title_info(view, selected->data, x1, y1, x2, y2);
    }
}

static void titles_update(ui_view* view, void* data, list_item** items, u32** itemCount, list_item* selected, bool selectedTouched) {
    if(hidKeysDown() & KEY_B) {
        list_destroy(view);
        ui_pop();
        return;
    }

    if(hidKeysDown() & KEY_X) {
        task_refresh_titles();
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        ui_push(titles_action_create((title_info*) selected->data));
    }

    if(*itemCount != task_get_title_count() || *items != task_get_titles()) {
        *itemCount = task_get_title_count();
        *items = task_get_titles();
    }
}

void titles_open() {
    ui_push(list_create("Titles", "A: Select, B: Return, X: Refresh", NULL, titles_update, titles_draw_top));
}