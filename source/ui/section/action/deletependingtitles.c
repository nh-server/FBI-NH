#include <malloc.h>
#include <stdio.h>

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

    bool all;

    data_op_info deleteInfo;
    Handle cancelEvent;
} delete_pending_titles_data;

static Result action_delete_pending_titles_delete(void* data, u32 index) {
    delete_pending_titles_data* deleteData = (delete_pending_titles_data*) data;

    list_item* item = deleteData->all ? (list_item*) linked_list_get(deleteData->items, index) : deleteData->selected;
    pending_title_info* info = (pending_title_info*) item->data;

    Result res = 0;

    if(R_SUCCEEDED(res = AM_DeletePendingTitle(info->mediaType, info->titleId))) {
        linked_list_remove(deleteData->items, item);
        task_free_pending_title(item);
    }

    return res;
}

static bool action_delete_pending_titles_error(void* data, u32 index, Result res) {
    delete_pending_titles_data* deleteData = (delete_pending_titles_data*) data;

    pending_title_info* info = (pending_title_info*) (deleteData->all ? ((list_item*) linked_list_get(deleteData->items, index))->data : deleteData->selected->data);

    if(res == R_FBI_CANCELLED) {
        prompt_display("Failure", "Delete cancelled.", COLOR_TEXT, false, info, NULL, ui_draw_pending_title_info, NULL);
        return false;
    } else {
        volatile bool dismissed = false;
        error_display_res(&dismissed, info, ui_draw_pending_title_info, res, "Failed to delete pending title.");

        while(!dismissed) {
            svcSleepThread(1000000);
        }
    }

    return index < deleteData->deleteInfo.total - 1;
}

static void action_delete_pending_titles_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    delete_pending_titles_data* deleteData = (delete_pending_titles_data*) data;

    u32 index = deleteData->deleteInfo.processed;
    if(index < deleteData->deleteInfo.total) {
        ui_draw_pending_title_info(view, (pending_title_info*) (deleteData->all ? ((list_item*) linked_list_get(deleteData->items, index))->data : deleteData->selected->data), x1, y1, x2, y2);
    }
}

static void action_delete_pending_titles_update(ui_view* view, void* data, float* progress, char* text) {
    delete_pending_titles_data* deleteData = (delete_pending_titles_data*) data;

    if(deleteData->deleteInfo.finished) {
        ui_pop();
        info_destroy(view);

        if(!deleteData->deleteInfo.premature) {
            prompt_display("Success", "Pending title(s) deleted.", COLOR_TEXT, false, NULL, NULL, NULL, NULL);
        }

        free(deleteData);

        return;
    }

    if(hidKeysDown() & KEY_B) {
        svcSignalEvent(deleteData->cancelEvent);
    }

    *progress = deleteData->deleteInfo.total > 0 ? (float) deleteData->deleteInfo.processed / (float) deleteData->deleteInfo.total : 0;
    snprintf(text, PROGRESS_TEXT_MAX, "%lu / %lu", deleteData->deleteInfo.processed, deleteData->deleteInfo.total);
}

static void action_delete_pending_titles_onresponse(ui_view* view, void* data, bool response) {
    delete_pending_titles_data* deleteData = (delete_pending_titles_data*) data;

    if(response) {
        deleteData->cancelEvent = task_data_op(&deleteData->deleteInfo);
        if(deleteData->cancelEvent != 0) {
            info_display("Deleting Pending Title(s)", "Press B to cancel.", true, data, action_delete_pending_titles_update, action_delete_pending_titles_draw_top);
        } else {
            error_display(NULL, NULL, NULL, "Failed to initiate delete operation.");
        }
    } else {
        free(deleteData);
    }
}

void action_delete_pending_titles(linked_list* items, list_item* selected, const char* message, bool all) {
    delete_pending_titles_data* data = (delete_pending_titles_data*) calloc(1, sizeof(delete_pending_titles_data));
    if(data == NULL) {
        error_display(NULL, NULL, NULL, "Failed to allocate delete pending titles data.");

        return;
    }

    data->items = items;
    data->selected = selected;

    data->all = all;

    data->deleteInfo.data = data;

    data->deleteInfo.op = DATAOP_DELETE;

    data->deleteInfo.total = all ? linked_list_size(items) : 1;

    data->deleteInfo.delete = action_delete_pending_titles_delete;

    data->deleteInfo.error = action_delete_pending_titles_error;

    data->cancelEvent = 0;

    prompt_display("Confirmation", message, COLOR_TEXT, true, data, NULL, !all ? action_delete_pending_titles_draw_top : NULL, action_delete_pending_titles_onresponse);
}

void action_delete_pending_title(linked_list* items, list_item* selected) {
    action_delete_pending_titles(items, selected, "Delete the selected pending title?", false);
}

void action_delete_all_pending_titles(linked_list* items, list_item* selected) {
    action_delete_pending_titles(items, selected, "Delete all pending titles?", true);
}