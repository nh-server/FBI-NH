#include <malloc.h>
#include <stdio.h>

#include <3ds.h>

#include "section.h"
#include "action/action.h"
#include "task/task.h"
#include "../error.h"
#include "../list.h"
#include "../ui.h"
#include "../../core/linkedlist.h"
#include "../../core/screen.h"

static list_item browse_user_save_data = {"Browse User Save Data", COLOR_TEXT, action_browse_user_ext_save_data};
static list_item browse_spotpass_save_data = {"Browse SpotPass Save Data", COLOR_TEXT, action_browse_boss_ext_save_data};
static list_item delete_save_data = {"Delete Save Data", COLOR_TEXT, action_delete_ext_save_data};

typedef struct {
    Handle cancelEvent;
    bool populated;
} extsavedata_data;

typedef struct {
    linked_list* items;
    list_item* selected;
} extsavedata_action_data;

static void extsavedata_action_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    ui_draw_ext_save_data_info(view, ((extsavedata_action_data*) data)->selected->data, x1, y1, x2, y2);
}

static void extsavedata_action_update(ui_view* view, void* data, linked_list* items, list_item* selected, bool selectedTouched) {
    extsavedata_action_data* actionData = (extsavedata_action_data*) data;

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
        linked_list_add(items, &browse_user_save_data);
        linked_list_add(items, &browse_spotpass_save_data);
        linked_list_add(items, &delete_save_data);
    }
}

static void extsavedata_action_open(linked_list* items, list_item* selected) {
    extsavedata_action_data* data = (extsavedata_action_data*) calloc(1, sizeof(extsavedata_action_data));
    if(data == NULL) {
        error_display(NULL, NULL, NULL, "Failed to allocate ext save data action data.");

        return;
    }

    data->items = items;
    data->selected = selected;

    list_display("Ext Save Data Action", "A: Select, B: Return", data, extsavedata_action_update, extsavedata_action_draw_top);
}

static void extsavedata_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    if(selected != NULL && selected->data != NULL) {
        ui_draw_ext_save_data_info(view, selected->data, x1, y1, x2, y2);
    }
}

static void extsavedata_update(ui_view* view, void* data, linked_list* items, list_item* selected, bool selectedTouched) {
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

        task_clear_ext_save_data(items);
        list_destroy(view);

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

        listData->cancelEvent = task_populate_ext_save_data(items);
        listData->populated = true;
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        extsavedata_action_open(items, selected);
        return;
    }
}

void extsavedata_open() {
    extsavedata_data* data = (extsavedata_data*) calloc(1, sizeof(extsavedata_data));
    if(data == NULL) {
        error_display(NULL, NULL, NULL, "Failed to allocate ext save data data.");

        return;
    }

    list_display("Ext Save Data", "A: Select, B: Return, X: Refresh", data, extsavedata_update, extsavedata_draw_top);
}