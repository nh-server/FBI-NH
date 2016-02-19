#include <malloc.h>
#include <stdio.h>

#include <3ds.h>

#include "action/action.h"
#include "task/task.h"
#include "section.h"

#define TITLES_MAX 1024

typedef struct {
    list_item items[TITLES_MAX];
    u32 count;
    Handle cancelEvent;
    bool populated;
} titles_data;

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
    titles_data* listData = (titles_data*) data;

    if(hidKeysDown() & KEY_B) {
        ui_pop();
        free(listData);
        list_destroy(view);
        return;
    }

    if(!listData->populated || (hidKeysDown() & KEY_X)) {
        if(listData->cancelEvent != 0) {
            svcSignalEvent(listData->cancelEvent);
            while(svcWaitSynchronization(listData->cancelEvent, 0) == 0) {
                svcSleepThread(1000000);
            }

            listData->cancelEvent = 0;
        }

        listData->cancelEvent = task_populate_titles(listData->items, &listData->count, TITLES_MAX);
        listData->populated = true;
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        listData->populated = false;

        ui_push(titles_action_create((title_info*) selected->data));
        return;
    }

    if(*itemCount != &listData->count || *items != listData->items) {
        *itemCount = &listData->count;
        *items = listData->items;
    }
}

void titles_open() {
    titles_data* data = (titles_data*) calloc(1, sizeof(titles_data));

    ui_push(list_create("Titles", "A: Select, B: Return, X: Refresh", data, titles_update, titles_draw_top));
}