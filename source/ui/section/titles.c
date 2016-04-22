#include <malloc.h>
#include <stdio.h>

#include <3ds.h>

#include "action/action.h"
#include "task/task.h"
#include "../../screen.h"
#include "section.h"

#define TITLES_MAX 1024

typedef struct {
    list_item items[TITLES_MAX];
    u32 count;
    Handle cancelEvent;
    bool populated;
} titles_data;

#define TITLES_ACTION_COUNT 6

static u32 titles_action_count = TITLES_ACTION_COUNT;
static list_item titles_action_items[TITLES_ACTION_COUNT] = {
        {"Launch Title", COLOR_TEXT, action_launch_title},
        {"Delete Title", COLOR_TEXT, action_delete_title},
        {"Browse Save Data", COLOR_TEXT, action_browse_title_save_data},
        {"Import Secure Value", COLOR_TEXT, action_import_secure_value},
        {"Export Secure Value", COLOR_TEXT, action_export_secure_value},
        {"Delete Secure Value", COLOR_TEXT, action_delete_secure_value},
};

#define CARD_TITLES_ACTION_COUNT 2

static u32 card_titles_action_count = CARD_TITLES_ACTION_COUNT;
static list_item card_titles_action_items[CARD_TITLES_ACTION_COUNT] = {
        {"Launch Title", COLOR_TEXT, action_launch_title},
        {"Browse Save Data", COLOR_TEXT, action_browse_title_save_data},
};

#define DSIWARE_TITLES_ACTION_COUNT 2

static u32 dsiware_titles_action_count = DSIWARE_TITLES_ACTION_COUNT;
static list_item dsiware_titles_action_items[DSIWARE_TITLES_ACTION_COUNT] = {
        {"Launch Title", COLOR_TEXT, action_launch_title},
        {"Delete Title", COLOR_TEXT, action_delete_title},
};

#define DSIWARE_CARD_TITLES_ACTION_COUNT 1

static u32 dsiware_card_titles_action_count = DSIWARE_CARD_TITLES_ACTION_COUNT;
static list_item dsiware_card_titles_action_items[DSIWARE_CARD_TITLES_ACTION_COUNT] = {
        {"Launch Title", COLOR_TEXT, action_launch_title},
};

typedef struct {
    title_info* info;
    bool* populated;
} titles_action_data;

static void titles_action_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    ui_draw_title_info(view, ((titles_action_data*) data)->info, x1, y1, x2, y2);
}

static void titles_action_update(ui_view* view, void* data, list_item** items, u32** itemCount, list_item* selected, bool selectedTouched) {
    titles_action_data* actionData = (titles_action_data*) data;

    if(hidKeysDown() & KEY_B) {
        ui_pop();
        list_destroy(view);

        free(data);

        return;
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        void(*action)(title_info*, bool*) = (void(*)(title_info*, bool*)) selected->data;

        ui_pop();
        list_destroy(view);

        action(actionData->info, actionData->populated);

        free(data);

        return;
    }

    if(actionData->info->mediaType == MEDIATYPE_GAME_CARD && actionData->info->twl) {
        if(*itemCount != &dsiware_card_titles_action_count || *items != dsiware_card_titles_action_items) {
            *itemCount = &dsiware_card_titles_action_count;
            *items = dsiware_card_titles_action_items;
        }
    } else if(actionData->info->mediaType == MEDIATYPE_GAME_CARD) {
        if(*itemCount != &card_titles_action_count || *items != card_titles_action_items) {
            *itemCount = &card_titles_action_count;
            *items = card_titles_action_items;
        }
    } else if(actionData->info->twl) {
        if(*itemCount != &dsiware_titles_action_count || *items != dsiware_titles_action_items) {
            *itemCount = &dsiware_titles_action_count;
            *items = dsiware_titles_action_items;
        }
    } else {
        if(*itemCount != &titles_action_count || *items != titles_action_items) {
            *itemCount = &titles_action_count;
            *items = titles_action_items;
        }
    }
}

static void titles_action_open(title_info* info, bool* populated) {
    titles_action_data* data = (titles_action_data*) calloc(1, sizeof(titles_action_data));
    data->info = info;
    data->populated = populated;

    list_display("Title Action", "A: Select, B: Return", data, titles_action_update, titles_action_draw_top);
}

static void titles_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    if(selected != NULL && selected->data != NULL) {
        ui_draw_title_info(view, selected->data, x1, y1, x2, y2);
    }
}

static void titles_update(ui_view* view, void* data, list_item** items, u32** itemCount, list_item* selected, bool selectedTouched) {
    titles_data* listData = (titles_data*) data;

    if(hidKeysDown() & KEY_B) {
        if(listData->cancelEvent != 0) {
            svcSignalEvent(listData->cancelEvent);
            while(svcWaitSynchronization(listData->cancelEvent, 0) == 0) {
                svcSleepThread(1000000);
            }

            listData->cancelEvent = 0;
        }

        ui_pop();
        list_destroy(view);

        task_clear_titles(listData->items, &listData->count);
        free(listData);
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
        titles_action_open((title_info*) selected->data, &listData->populated);
        return;
    }

    if(*itemCount != &listData->count || *items != listData->items) {
        *itemCount = &listData->count;
        *items = listData->items;
    }
}

void titles_open() {
    titles_data* data = (titles_data*) calloc(1, sizeof(titles_data));

    list_display("Titles", "A: Select, B: Return, X: Refresh", data, titles_update, titles_draw_top);
}