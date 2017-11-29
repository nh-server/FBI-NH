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

static titledb_type section_cia_type = TITLEDB_TYPE_CIA;
static list_item section_cia = {"CIA", COLOR_TEXT, &section_cia_type};
static titledb_type section_3dsx_type = TITLEDB_TYPE_3DSX;
static list_item section_3dsx = {"3DSX", COLOR_TEXT, &section_3dsx_type};

static list_item action_install = {"Install", COLOR_TEXT, action_install_titledb};

typedef struct {
    populate_titledb_data populateData;

    bool populated;
} titledb_section_data;

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
        linked_list_add(items, &action_install);
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

static void titledb_section_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    titledb_section_data* listData = (titledb_section_data*) data;

    if(!listData->populateData.itemsListed) {
        static const char* text = "Loading title list, please wait...\nNOTE: Cancelling may take up to 15 seconds.";

        float textWidth;
        float textHeight;
        screen_get_string_size(&textWidth, &textHeight, text, 0.5f, 0.5f);
        screen_draw_string(text, x1 + (x2 - x1 - textWidth) / 2, y1 + (y2 - y1 - textHeight) / 2, 0.5f, 0.5f, COLOR_TEXT, true);
    } else if(selected != NULL && selected->data != NULL) {
        ui_draw_titledb_info(view, selected->data, x1, y1, x2, y2);
    }
}

static void titledb_section_update(ui_view* view, void* data, linked_list* items, list_item* selected, bool selectedTouched) {
    titledb_section_data* listData = (titledb_section_data*) data;

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

void titledb_section_open(titledb_type type) {
    titledb_section_data* data = (titledb_section_data*) calloc(1, sizeof(titledb_section_data));
    if(data == NULL) {
        error_display(NULL, NULL, "Failed to allocate TitleDB section data.");

        return;
    }

    data->populateData.type = type;

    data->populateData.finished = true;

    list_display("TitleDB.com", "A: Select, B: Return, X: Refresh", data, titledb_section_update, titledb_section_draw_top);
}

static void titledb_update(ui_view* view, void* data, linked_list* items, list_item* selected, bool selectedTouched) {
    if(hidKeysDown() & KEY_B) {
        ui_pop();
        list_destroy(view);

        return;
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        titledb_section_open(*(titledb_type*) selected->data);
        return;
    }

    if(linked_list_size(items) == 0) {
        linked_list_add(items, &section_cia);
        linked_list_add(items, &section_3dsx);
    }
}

void titledb_open() {
    list_display("TitleDB.com", "A: Select, B: Return", NULL, titledb_update, NULL);
}