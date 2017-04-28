#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <3ds.h>

#include "action.h"
#include "../task/task.h"
#include "../../error.h"
#include "../../info.h"
#include "../../list.h"
#include "../../prompt.h"
#include "../../ui.h"
#include "../../../core/linkedlist.h"
#include "../../../core/screen.h"

typedef struct {
    linked_list* items;
    list_item* selected;

    linked_list contents;
    bool all;

    data_op_data deleteInfo;
} delete_pending_titles_data;

static void action_delete_pending_titles_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    delete_pending_titles_data* deleteData = (delete_pending_titles_data*) data;

    u32 index = deleteData->deleteInfo.processed;
    if(index < deleteData->deleteInfo.total) {
        ui_draw_pending_title_info(view, (pending_title_info*) ((list_item*) linked_list_get(&deleteData->contents, index))->data, x1, y1, x2, y2);
    }
}

static Result action_delete_pending_titles_delete(void* data, u32 index) {
    delete_pending_titles_data* deleteData = (delete_pending_titles_data*) data;

    list_item* item = (list_item*) linked_list_get(&deleteData->contents, index);
    pending_title_info* info = (pending_title_info*) item->data;

    Result res = 0;

    if(R_SUCCEEDED(res = AM_DeletePendingTitle(info->mediaType, info->titleId))) {
        linked_list_iter iter;
        linked_list_iterate(deleteData->items, &iter);

        while(linked_list_iter_has_next(&iter)) {
            list_item* currItem = (list_item*) linked_list_iter_next(&iter);
            pending_title_info* currInfo = (pending_title_info*) currItem->data;

            if(currInfo->titleId == info->titleId && currInfo->mediaType == info->mediaType) {
                linked_list_iter_remove(&iter);
                task_free_pending_title(currItem);
            }
        }
    }

    return res;
}

static Result action_delete_pending_titles_suspend(void* data, u32 index) {
    return 0;
}

static Result action_delete_pending_titles_restore(void* data, u32 index) {
    return 0;
}

static bool action_delete_pending_titles_error(void* data, u32 index, Result res) {
    delete_pending_titles_data* deleteData = (delete_pending_titles_data*) data;

    if(res == R_FBI_CANCELLED) {
        prompt_display_notify("Failure", "Delete cancelled.", COLOR_TEXT, NULL, NULL, NULL);
        return false;
    } else {
        ui_view* view = error_display_res(data, action_delete_pending_titles_draw_top, res, "Failed to delete pending title.");
        if(view != NULL) {
            svcWaitSynchronization(view->active, U64_MAX);
        }
    }

    return index < deleteData->deleteInfo.total - 1;
}

static void action_delete_pending_titles_free_data(delete_pending_titles_data* data) {
    if(data->all) {
        task_clear_pending_titles(&data->contents);
    }

    linked_list_destroy(&data->contents);
    free(data);
}

static void action_delete_pending_titles_update(ui_view* view, void* data, float* progress, char* text) {
    delete_pending_titles_data* deleteData = (delete_pending_titles_data*) data;

    if(deleteData->deleteInfo.finished) {
        ui_pop();
        info_destroy(view);

        if(R_SUCCEEDED(deleteData->deleteInfo.result)) {
            prompt_display_notify("Success", "Pending title(s) deleted.", COLOR_TEXT, NULL, NULL, NULL);
        }

        action_delete_pending_titles_free_data(deleteData);

        return;
    }

    if((hidKeysDown() & KEY_B) && !deleteData->deleteInfo.finished) {
        svcSignalEvent(deleteData->deleteInfo.cancelEvent);
    }

    *progress = deleteData->deleteInfo.total > 0 ? (float) deleteData->deleteInfo.processed / (float) deleteData->deleteInfo.total : 0;
    snprintf(text, PROGRESS_TEXT_MAX, "%lu / %lu", deleteData->deleteInfo.processed, deleteData->deleteInfo.total);
}

static void action_delete_pending_titles_onresponse(ui_view* view, void* data, u32 response) {
    delete_pending_titles_data* deleteData = (delete_pending_titles_data*) data;

    if(response == PROMPT_YES) {
        Result res = task_data_op(&deleteData->deleteInfo);
        if(R_SUCCEEDED(res)) {
            info_display("Deleting Pending Title(s)", "Press B to cancel.", true, data, action_delete_pending_titles_update, action_delete_pending_titles_draw_top);
        } else {
            error_display_res(NULL, NULL, res, "Failed to initiate delete operation.");

            action_delete_pending_titles_free_data(deleteData);
        }
    } else {
        action_delete_pending_titles_free_data(deleteData);
    }
}

typedef struct {
    delete_pending_titles_data* deleteData;

    const char* message;

    populate_pending_titles_data popData;
} delete_pending_titles_loading_data;

static void action_delete_pending_titles_loading_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    action_delete_pending_titles_draw_top(view, ((delete_pending_titles_loading_data*) data)->deleteData, x1, y1, x2, y2);
}

static void action_delete_pending_titles_loading_update(ui_view* view, void* data, float* progress, char* text)  {
    delete_pending_titles_loading_data* loadingData = (delete_pending_titles_loading_data*) data;

    if(loadingData->popData.finished) {
        ui_pop();
        info_destroy(view);

        if(R_SUCCEEDED(loadingData->popData.result)) {
            loadingData->deleteData->deleteInfo.total = linked_list_size(&loadingData->deleteData->contents);
            loadingData->deleteData->deleteInfo.processed = loadingData->deleteData->deleteInfo.total;

            prompt_display_yes_no("Confirmation", loadingData->message, COLOR_TEXT, loadingData->deleteData, action_delete_pending_titles_draw_top, action_delete_pending_titles_onresponse);
        } else {
            error_display_res(NULL, NULL, loadingData->popData.result, "Failed to populate pending title list.");

            action_delete_pending_titles_free_data(loadingData->deleteData);
        }

        free(loadingData);
        return;
    }

    if((hidKeysDown() & KEY_B) && !loadingData->popData.finished) {
        svcSignalEvent(loadingData->popData.cancelEvent);
    }

    snprintf(text, PROGRESS_TEXT_MAX, "Fetching pending title list...");
}

void action_delete_pending_titles(linked_list* items, list_item* selected, const char* message, bool all) {
    delete_pending_titles_data* data = (delete_pending_titles_data*) calloc(1, sizeof(delete_pending_titles_data));
    if(data == NULL) {
        error_display(NULL, NULL, "Failed to allocate delete pending titles data.");

        return;
    }

    data->items = items;
    data->selected = selected;

    data->all = all;

    data->deleteInfo.data = data;

    data->deleteInfo.op = DATAOP_DELETE;

    data->deleteInfo.delete = action_delete_pending_titles_delete;

    data->deleteInfo.suspend = action_delete_pending_titles_suspend;
    data->deleteInfo.restore = action_delete_pending_titles_restore;

    data->deleteInfo.error = action_delete_pending_titles_error;

    data->deleteInfo.finished = true;

    linked_list_init(&data->contents);

    if(all) {
        delete_pending_titles_loading_data* loadingData = (delete_pending_titles_loading_data*) calloc(1, sizeof(delete_pending_titles_loading_data));
        if(loadingData == NULL) {
            error_display(NULL, NULL, "Failed to allocate loading data.");

            action_delete_pending_titles_free_data(data);
            return;
        }

        loadingData->deleteData = data;
        loadingData->message = message;

        loadingData->popData.items = &data->contents;

        Result listRes = task_populate_pending_titles(&loadingData->popData);
        if(R_FAILED(listRes)) {
            error_display_res(NULL, NULL, listRes, "Failed to initiate pending title list population.");

            free(loadingData);
            action_delete_pending_titles_free_data(data);
            return;
        }

        info_display("Loading", "Press B to cancel.", false, loadingData, action_delete_pending_titles_loading_update, action_delete_pending_titles_loading_draw_top);
    } else {
        linked_list_add(&data->contents, selected);

        data->deleteInfo.total = 1;
        data->deleteInfo.processed = data->deleteInfo.total;

        prompt_display_yes_no("Confirmation", message, COLOR_TEXT, data, action_delete_pending_titles_draw_top, action_delete_pending_titles_onresponse);
    }
}

void action_delete_pending_title(linked_list* items, list_item* selected) {
    action_delete_pending_titles(items, selected, "Delete the selected pending title?", false);
}

void action_delete_all_pending_titles(linked_list* items, list_item* selected) {
    action_delete_pending_titles(items, selected, "Delete all pending titles?", true);
}