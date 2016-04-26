#include <malloc.h>
#include <stdio.h>

#include <3ds.h>

#include "action/action.h"
#include "section.h"
#include "../error.h"
#include "../list.h"
#include "../../screen.h"

static list_item delete_pending_title = {"Delete Pending Title", COLOR_TEXT, action_delete_pending_title};
static list_item delete_all_pending_titles = {"Delete All Pending Titles", COLOR_TEXT, action_delete_all_pending_titles};

typedef struct {
    Handle cancelEvent;
    bool populated;
} pendingtitles_data;

typedef struct {
    pending_title_info* info;
    bool* populated;
} pendingtitles_action_data;

static void pendingtitles_action_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    ui_draw_pending_title_info(view, ((pendingtitles_action_data*) data)->info, x1, y1, x2, y2);
}

static void pendingtitles_action_update(ui_view* view, void* data, linked_list* items, list_item* selected, bool selectedTouched) {
    pendingtitles_action_data* actionData = (pendingtitles_action_data*) data;

    if(hidKeysDown() & KEY_B) {
        ui_pop();
        list_destroy(view);

        free(data);

        return;
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        void(*action)(pending_title_info*, bool*) = (void(*)(pending_title_info*, bool*)) selected->data;

        ui_pop();
        list_destroy(view);

        action(actionData->info, actionData->populated);

        free(data);

        return;
    }

    if(linked_list_size(items) == 0) {
        linked_list_add(items, &delete_pending_title);
        linked_list_add(items, &delete_all_pending_titles);
    }
}

static void pendingtitles_action_open(pending_title_info* info, bool* populated) {
    pendingtitles_action_data* data = (pendingtitles_action_data*) calloc(1, sizeof(pendingtitles_action_data));
    if(data == NULL) {
        error_display(NULL, NULL, NULL, "Failed to allocate pending titles action data.");

        return;
    }

    data->info = info;
    data->populated = populated;

    list_display("Pending Title Action", "A: Select, B: Return", data, pendingtitles_action_update, pendingtitles_action_draw_top);
}

static void pendingtitles_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    if(selected != NULL && selected->data != NULL) {
        ui_draw_pending_title_info(view, selected->data, x1, y1, x2, y2);
    }
}

static void pendingtitles_update(ui_view* view, void* data, linked_list* items, list_item* selected, bool selectedTouched) {
    pendingtitles_data* listData = (pendingtitles_data*) data;

    if(hidKeysDown() & KEY_B) {
        if(listData->cancelEvent != 0) {
            svcSignalEvent(listData->cancelEvent);
            while(svcWaitSynchronization(listData->cancelEvent, 0) == 0) {
                svcSleepThread(1000000);
            }

            listData->cancelEvent = 0;
        }

        ui_pop();

        task_clear_pending_titles(items);
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

        listData->cancelEvent = task_populate_pending_titles(items);
        listData->populated = true;
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        pendingtitles_action_open((pending_title_info*) selected->data, &listData->populated);
        return;
    }
}

void pendingtitles_open() {
    pendingtitles_data* data = (pendingtitles_data*) calloc(1, sizeof(pendingtitles_data));
    if(data == NULL) {
        error_display(NULL, NULL, NULL, "Failed to allocate pending titles data.");

        return;
    }

    list_display("Pending Titles", "A: Select, B: Return, X: Refresh", data, pendingtitles_update, pendingtitles_draw_top);
}