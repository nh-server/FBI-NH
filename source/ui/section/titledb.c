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

static list_item install = {"Install", COLOR_TEXT, action_install_titledb};

// TODO: Updating disabled pending TitleDB pull request.
//static list_item update_all = {"Update All", COLOR_TEXT, action_update_titledb};

typedef struct {
    populate_titledb_data populateData;

    bool populated;
} titledb_data;

typedef struct {
    linked_list* items;
    list_item* selected;
} titledb_action_data;

static void titledb_action_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    ui_draw_titledb_info(view, ((titledb_action_data*) data)->selected->data, x1, y1, x2, y2);
}

static void titledb_action_update(ui_view* view, void* data, linked_list* items, list_item* selected, bool selectedTouched) {
    titledb_action_data* actionData = (titledb_action_data*) data;

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
        linked_list_add(items, &install);

        // TODO: Updating disabled pending TitleDB pull request.
        //linked_list_add(items, &update_all);
    }
}

static void titledb_action_open(linked_list* items, list_item* selected) {
    titledb_action_data* data = (titledb_action_data*) calloc(1, sizeof(titledb_action_data));
    if(data == NULL) {
        error_display(NULL, NULL, "Failed to allocate TitleDB action data.");

        return;
    }

    data->items = items;
    data->selected = selected;

    list_display("TitleDB Action", "A: Select, B: Return", data, titledb_action_update, titledb_action_draw_top);
}

static void titledb_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    if(selected != NULL && selected->data != NULL) {
        ui_draw_titledb_info(view, selected->data, x1, y1, x2, y2);
    }
}

static void titledb_update(ui_view* view, void* data, linked_list* items, list_item* selected, bool selectedTouched) {
    titledb_data* listData = (titledb_data*) data;

    if(hidKeysDown() & KEY_B) {
        if(!listData->populateData.finished) {
            svcSignalEvent(listData->populateData.cancelEvent);
            while(!listData->populateData.finished) {
                svcSleepThread(1000000);
            }
        }

        ui_pop();

        task_clear_titledb(items);
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
        Result res = task_populate_titledb(&listData->populateData);
        if(R_FAILED(res)) {
            error_display_res(NULL, NULL, res, "Failed to initiate TitleDB list population.");
        }

        listData->populated = true;
    }

    if(listData->populateData.finished && R_FAILED(listData->populateData.result)) {
        error_display_res(NULL, NULL, listData->populateData.result, "Failed to populate TitleDB list.");

        listData->populateData.result = 0;
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        titledb_action_open(items, selected);
        return;
    }
}

void titledb_open() {
    titledb_data* data = (titledb_data*) calloc(1, sizeof(titledb_data));
    if(data == NULL) {
        error_display(NULL, NULL, "Failed to allocate TitleDB data.");

        return;
    }

    data->populateData.finished = true;

    list_display("TitleDB.com", "A: Select, B: Return, X: Refresh", data, titledb_update, titledb_draw_top);
}