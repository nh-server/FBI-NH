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
    populate_ext_save_data_data populateData;

    bool showSD;
    bool showNAND;

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
        error_display(NULL, NULL, "Failed to allocate ext save data action data.");

        return;
    }

    data->items = items;
    data->selected = selected;

    list_display("Ext Save Data Action", "A: Select, B: Return", data, extsavedata_action_update, extsavedata_action_draw_top);
}

static void extsavedata_filters_add_entry(linked_list* items, const char* name, bool* val) {
    list_item* item = (list_item*) calloc(1, sizeof(list_item));
    if(item != NULL) {
        snprintf(item->name, LIST_ITEM_NAME_MAX, "%s", name);
        item->color = *val ? COLOR_ENABLED : COLOR_DISABLED;
        item->data = val;

        linked_list_add(items, item);
    }
}

static void extsavedata_filters_update(ui_view* view, void* data, linked_list* items, list_item* selected, bool selectedTouched) {
    extsavedata_data* listData = (extsavedata_data*) data;

    if(hidKeysDown() & KEY_B) {
        linked_list_iter iter;
        linked_list_iterate(items, &iter);

        while(linked_list_iter_has_next(&iter)) {
            free(linked_list_iter_next(&iter));
            linked_list_iter_remove(&iter);
        }

        ui_pop();
        list_destroy(view);

        return;
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        bool* val = (bool*) selected->data;
        *val = !(*val);

        selected->color = *val ? COLOR_ENABLED : COLOR_DISABLED;

        listData->populated = false;
    }

    if(linked_list_size(items) == 0) {
        extsavedata_filters_add_entry(items, "Show SD", &listData->showSD);
        extsavedata_filters_add_entry(items, "Show NAND", &listData->showNAND);
    }
}

static void extsavedata_filters_open(extsavedata_data* data) {
    list_display("Filters", "A: Toggle, B: Return", data, extsavedata_filters_update, NULL);
}

static void extsavedata_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    if(selected != NULL && selected->data != NULL) {
        ui_draw_ext_save_data_info(view, selected->data, x1, y1, x2, y2);
    }
}

static void extsavedata_update(ui_view* view, void* data, linked_list* items, list_item* selected, bool selectedTouched) {
    extsavedata_data* listData = (extsavedata_data*) data;

    if(hidKeysDown() & KEY_B) {
        if(!listData->populateData.finished) {
            svcSignalEvent(listData->populateData.cancelEvent);
            while(!listData->populateData.finished) {
                svcSleepThread(1000000);
            }
        }

        ui_pop();

        task_clear_ext_save_data(items);
        list_destroy(view);

        free(listData);
        return;
    }

    if(hidKeysDown() & KEY_SELECT) {
        extsavedata_filters_open(listData);
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
        Result res = task_populate_ext_save_data(&listData->populateData);
        if(R_FAILED(res)) {
            error_display_res(NULL, NULL, res, "Failed to initiate ext save data list population.");
        }

        listData->populated = true;
    }

    if(listData->populateData.finished && R_FAILED(listData->populateData.result)) {
        error_display_res(NULL, NULL, listData->populateData.result, "Failed to populate ext save data list.");

        listData->populateData.result = 0;
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        extsavedata_action_open(items, selected);
        return;
    }
}

static bool extsavedata_filter(void* data, u64 titleId, FS_MediaType mediaType) {
    extsavedata_data* listData = (extsavedata_data*) data;

    if(mediaType == MEDIATYPE_SD) {
        return listData->showSD;
    } else {
        return listData->showNAND;
    }
}

void extsavedata_open() {
    extsavedata_data* data = (extsavedata_data*) calloc(1, sizeof(extsavedata_data));
    if(data == NULL) {
        error_display(NULL, NULL, "Failed to allocate ext save data data.");

        return;
    }

    data->populateData.filter = extsavedata_filter;
    data->populateData.filterData = data;

    data->populateData.finished = true;

    data->showSD = true;
    data->showNAND = true;

    list_display("Ext Save Data", "A: Select, B: Return, X: Refresh, Select: Filter", data, extsavedata_update, extsavedata_draw_top);
}