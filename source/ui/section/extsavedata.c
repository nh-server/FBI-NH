#include <malloc.h>
#include <stdio.h>

#include <3ds.h>

#include "action/action.h"
#include "task/task.h"
#include "../../screen.h"
#include "section.h"

#define EXTSAVEDATA_MAX 512

typedef struct {
    list_item items[EXTSAVEDATA_MAX];
    u32 count;
    Handle cancelEvent;
    bool populated;
} extsavedata_data;

#define EXTSAVEDATA_ACTION_COUNT 3

static u32 extsavedata_action_count = EXTSAVEDATA_ACTION_COUNT;
static list_item extsavedata_action_items[EXTSAVEDATA_ACTION_COUNT] = {
        {"Browse User Save Data", COLOR_TEXT, action_browse_user_ext_save_data},
        {"Browse SpotPass Save Data", COLOR_TEXT, action_browse_boss_ext_save_data},
        {"Delete Save Data", COLOR_TEXT, action_delete_ext_save_data},
};

typedef struct {
    ext_save_data_info* info;
    bool* populated;
} extsavedata_action_data;

static void extsavedata_action_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    ui_draw_ext_save_data_info(view, ((extsavedata_action_data*) data)->info, x1, y1, x2, y2);
}

static void extsavedata_action_update(ui_view* view, void* data, list_item** items, u32** itemCount, list_item* selected, bool selectedTouched) {
    extsavedata_action_data* actionData = (extsavedata_action_data*) data;

    if(hidKeysDown() & KEY_B) {
        ui_pop();
        list_destroy(view);

        free(data);

        return;
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        void(*action)(ext_save_data_info*, bool*) = (void(*)(ext_save_data_info*, bool*)) selected->data;

        ui_pop();
        list_destroy(view);

        action(actionData->info, actionData->populated);

        free(data);

        return;
    }

    if(*itemCount != &extsavedata_action_count || *items != extsavedata_action_items) {
        *itemCount = &extsavedata_action_count;
        *items = extsavedata_action_items;
    }
}

static void extsavedata_action_open(ext_save_data_info* info, bool* populated) {
    extsavedata_action_data* data = (extsavedata_action_data*) calloc(1, sizeof(extsavedata_action_data));
    data->info = info;
    data->populated = populated;

    list_display("Ext Save Data Action", "A: Select, B: Return", data, extsavedata_action_update, extsavedata_action_draw_top);
}

static void extsavedata_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    if(selected != NULL && selected->data != NULL) {
        ui_draw_ext_save_data_info(view, selected->data, x1, y1, x2, y2);
    }
}

static void extsavedata_update(ui_view* view, void* data, list_item** items, u32** itemCount, list_item* selected, bool selectedTouched) {
    extsavedata_data* listData = (extsavedata_data*) data;

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

        task_clear_ext_save_data(listData->items, &listData->count);
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

        listData->cancelEvent = task_populate_ext_save_data(listData->items, &listData->count, EXTSAVEDATA_MAX);
        listData->populated = true;
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        extsavedata_action_open((ext_save_data_info*) selected->data, &listData->populated);
        return;
    }

    if(*itemCount != &listData->count || *items != listData->items) {
        *itemCount = &listData->count;
        *items = listData->items;
    }
}

void extsavedata_open() {
    extsavedata_data* data = (extsavedata_data*) calloc(1, sizeof(extsavedata_data));

    list_display("Ext Save Data", "A: Select, B: Return, X: Refresh", data, extsavedata_update, extsavedata_draw_top);
}