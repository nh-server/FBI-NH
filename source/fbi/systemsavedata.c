#include <malloc.h>
#include <stdio.h>

#include <3ds.h>

#include "resources.h"
#include "section.h"
#include "action/action.h"
#include "task/uitask.h"
#include "../core/core.h"

static list_item browse_save_data = {"Browse Save Data", COLOR_TEXT, action_browse_system_save_data};
static list_item delete_save_data = {"Delete Save Data", COLOR_TEXT, action_delete_system_save_data};

typedef struct {
    populate_system_save_data_data populateData;

    bool populated;
} systemsavedata_data;

typedef struct {
    linked_list* items;
    list_item* selected;
} systemsavedata_action_data;

static void systemsavedata_action_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    task_draw_system_save_data_info(view, ((systemsavedata_action_data*) data)->selected->data, x1, y1, x2, y2);
}

static void systemsavedata_action_update(ui_view* view, void* data, linked_list* items, list_item* selected, bool selectedTouched) {
    systemsavedata_action_data* actionData = (systemsavedata_action_data*) data;

    if(hidKeysDown() & KEY_B) {
        ui_pop();
        list_destroy(view);

        free(data);

        return;
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        void(*action)(linked_list*, list_item*) = (void(*)(linked_list*, list_item*)) selected->data;

        ui_pop();
        list_destroy(view);

        action(actionData->items, actionData->selected);

        free(data);

        return;
    }

    if(linked_list_size(items) == 0) {
        linked_list_add(items, &browse_save_data);
        linked_list_add(items, &delete_save_data);
    }
}

static void systemsavedata_action_open(linked_list* items, list_item* selected) {
    systemsavedata_action_data* data = (systemsavedata_action_data*) calloc(1, sizeof(systemsavedata_action_data));
    if(data == NULL) {
        error_display(NULL, NULL, "Failed to allocate system save data action data.");

        return;
    }

    data->items = items;
    data->selected = selected;

    list_display("System Save Data Action", "A: Select, B: Return", data, systemsavedata_action_update, systemsavedata_action_draw_top);
}

static void systemsavedata_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    if(selected != NULL && selected->data != NULL) {
        task_draw_system_save_data_info(view, selected->data, x1, y1, x2, y2);
    }
}

static void systemsavedata_update(ui_view* view, void* data, linked_list* items, list_item* selected, bool selectedTouched) {
    systemsavedata_data* listData = (systemsavedata_data*) data;

    if(hidKeysDown() & KEY_B) {
        if(!listData->populateData.finished) {
            svcSignalEvent(listData->populateData.cancelEvent);
            while(!listData->populateData.finished) {
                svcSleepThread(1000000);
            }
        }

        ui_pop();

        task_clear_system_save_data(items);
        list_destroy(view);

        free(listData);
        return;
    }

    if(!listData->populated || (hidKeysDown() & KEY_X)) {
        if(!listData->populateData.finished) {
            svcSignalEvent(listData->populateData.cancelEvent);
            while(!listData->populateData.finished) {
                svcSleepThread(1000000);
            }
        }

        listData->populateData.items = items;
        Result res = task_populate_system_save_data(&listData->populateData);
        if(R_FAILED(res)) {
            error_display_res(NULL, NULL, res, "Failed to initiate system save data list population.");
        }

        listData->populated = true;
    }

    if(listData->populateData.finished && R_FAILED(listData->populateData.result)) {
        error_display_res(NULL, NULL, listData->populateData.result, "Failed to populate system save data list.");

        listData->populateData.result = 0;
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        systemsavedata_action_open(items, selected);
        return;
    }
}

void systemsavedata_open() {
    systemsavedata_data* data = (systemsavedata_data*) calloc(1, sizeof(systemsavedata_data));
    if(data == NULL) {
        error_display(NULL, NULL, "Failed to allocate system save data data.");

        return;
    }

    data->populateData.finished = true;

    list_display("System Save Data", "A: Select, B: Return, X: Refresh", data, systemsavedata_update, systemsavedata_draw_top);
}