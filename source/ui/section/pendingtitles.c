#include <malloc.h>
#include <stdio.h>

#include <3ds.h>

#include "action/action.h"
#include "section.h"

#define PENDINGTITLES_ACTION_COUNT 2

static u32 pending_titles_action_count = PENDINGTITLES_ACTION_COUNT;
static list_item pending_titles_action_items[PENDINGTITLES_ACTION_COUNT] = {
        {"Delete Pending Title", 0xFF000000, action_delete_pending_title},
        {"Delete All Pending Titles", 0xFF000000, action_delete_all_pending_titles},
};

static void pendingtitles_action_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    ui_draw_pending_title_info(view, data, x1, y1, x2, y2);
}

static void pendingtitles_action_update(ui_view* view, void* data, list_item** items, u32** itemCount, list_item* selected, bool selectedTouched) {
    if(hidKeysDown() & KEY_B) {
        list_destroy(view);
        ui_pop();
        return;
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        void(*action)(pending_title_info*) = (void(*)(pending_title_info*)) selected->data;

        list_destroy(view);
        ui_pop();

        action((pending_title_info*) data);
        return;
    }

    if(*itemCount != &pending_titles_action_count || *items != pending_titles_action_items) {
        *itemCount = &pending_titles_action_count;
        *items = pending_titles_action_items;
    }
}

static ui_view* pendingtitles_action_create(pending_title_info* info) {
    return list_create("Pending Title Action", "A: Select, B: Return", info, pendingtitles_action_update, pendingtitles_action_draw_top);
}

static void pendingtitles_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    if(selected != NULL && selected->data != NULL) {
        ui_draw_pending_title_info(view, selected->data, x1, y1, x2, y2);
    }
}

static void pendingtitles_update(ui_view* view, void* data, list_item** items, u32** itemCount, list_item* selected, bool selectedTouched) {
    if(hidKeysDown() & KEY_B) {
        list_destroy(view);
        ui_pop();
        return;
    }

    if(hidKeysDown() & KEY_X) {
        task_refresh_pending_titles();
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        ui_push(pendingtitles_action_create((pending_title_info*) selected->data));
    }

    if(*itemCount != task_get_pending_title_count() || *items != task_get_pending_titles()) {
        *itemCount = task_get_pending_title_count();
        *items = task_get_pending_titles();
    }
}

void pendingtitles_open() {
    ui_push(list_create("Pending Titles", "A: Select, B: Return, X: Refresh", NULL, pendingtitles_update, pendingtitles_draw_top));
}