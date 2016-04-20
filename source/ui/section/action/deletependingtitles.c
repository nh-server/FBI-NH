#include <malloc.h>
#include <stdio.h>

#include <3ds.h>

#include "action.h"
#include "../../error.h"
#include "../../info.h"
#include "../../prompt.h"
#include "../../../screen.h"

typedef struct {
    pending_title_info* info;
    bool* populated;
    u64* titleIds;

    u32 sdTotal;
    u32 nandTotal;

    data_op_info deleteInfo;
    Handle cancelEvent;
} delete_pending_titles_data;

static Result action_delete_pending_titles_delete(void* data, u32 index) {
    delete_pending_titles_data* deleteData = (delete_pending_titles_data*) data;

    Result res = 0;

    if(R_SUCCEEDED(res = AM_DeletePendingTitle(index >= deleteData->sdTotal ? MEDIATYPE_NAND : MEDIATYPE_SD, deleteData->titleIds[index]))) {
        *deleteData->populated = false;
    }

    return res;
}

static bool action_delete_pending_titles_error(void* data, u32 index, Result res) {
    delete_pending_titles_data* deleteData = (delete_pending_titles_data*) data;

    if(res == R_FBI_CANCELLED) {
        prompt_display("Failure", "Delete cancelled.", COLOR_TEXT, false, deleteData->info, NULL, deleteData->info != NULL ? ui_draw_pending_title_info : NULL, NULL);
        return false;
    } else {
        u64 titleId = deleteData->titleIds[index];

        volatile bool dismissed = false;
        error_display_res(&dismissed, deleteData->info, deleteData->info != NULL ? ui_draw_pending_title_info : NULL, res, "Failed to delete pending title.\n%llX", titleId);

        while(!dismissed) {
            svcSleepThread(1000000);
        }
    }

    return index < deleteData->deleteInfo.total - 1;
}

static void action_delete_pending_titles_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    delete_pending_titles_data* deleteData = (delete_pending_titles_data*) data;

    if(deleteData->info != NULL) {
        ui_draw_pending_title_info(view, deleteData->info, x1, y1, x2, y2);
    }
}

static void action_delete_pending_titles_free_data(delete_pending_titles_data* data) {
    free(data->titleIds);
    free(data);
}

static void action_delete_pending_titles_update(ui_view* view, void* data, float* progress, char* text) {
    delete_pending_titles_data* deleteData = (delete_pending_titles_data*) data;

    if(deleteData->deleteInfo.finished) {
        ui_pop();
        info_destroy(view);

        if(deleteData->deleteInfo.premature) {
            action_delete_pending_titles_free_data(deleteData);
        } else {
            prompt_display("Success", "Pending title(s) deleted.", COLOR_TEXT, false, deleteData->info, NULL, deleteData->info != NULL ? ui_draw_pending_title_info : NULL, NULL);
        }

        action_delete_pending_titles_free_data(deleteData);

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
        action_delete_pending_titles_free_data(deleteData);
    }
}

void action_delete_pending_titles(pending_title_info* info, bool* populated, const char* message, u64* titleIds, u32 sdTotal, u32 nandTotal) {
    delete_pending_titles_data* data = (delete_pending_titles_data*) calloc(1, sizeof(delete_pending_titles_data));
    data->info = info;
    data->populated = populated;
    data->titleIds = titleIds;
    data->sdTotal = sdTotal;
    data->nandTotal = nandTotal;

    data->deleteInfo.data = data;

    data->deleteInfo.op = DATAOP_DELETE;

    data->deleteInfo.total = sdTotal + nandTotal;

    data->deleteInfo.delete = action_delete_pending_titles_delete;

    data->deleteInfo.error = action_delete_pending_titles_error;

    data->cancelEvent = 0;

    prompt_display("Confirmation", message, COLOR_TEXT, true, data, NULL, action_delete_pending_titles_draw_top, action_delete_pending_titles_onresponse);
}

void action_delete_pending_title(pending_title_info* info, bool* populated) {
    u64* titleIds = (u64*) calloc(1, sizeof(u64));
    titleIds[0] = info->titleId;
    u32 sdTotal = info->mediaType == MEDIATYPE_SD ? 1 : 0;
    u32 nandTotal = info->mediaType == MEDIATYPE_NAND ? 1 : 0;

    action_delete_pending_titles(info, populated, "Delete the selected pending title?", titleIds, sdTotal, nandTotal);
}

void action_delete_all_pending_titles(pending_title_info* info, bool* populated) {
    u32 sdTotal = 0;
    u32 nandTotal = 0;

    Result res = 0;
    if(R_FAILED(res = AM_GetPendingTitleCount(&sdTotal, MEDIATYPE_SD, AM_STATUS_MASK_INSTALLING | AM_STATUS_MASK_AWAITING_FINALIZATION)) || R_FAILED(res = AM_GetPendingTitleCount(&nandTotal, MEDIATYPE_NAND, AM_STATUS_MASK_INSTALLING | AM_STATUS_MASK_AWAITING_FINALIZATION))) {
        error_display_res(NULL, NULL, NULL, res, "Failed to retrieve pending title count.");

        return;
    }

    u64* titleIds = (u64*) calloc(sdTotal + nandTotal, sizeof(u64));
    if(R_FAILED(res = AM_GetPendingTitleList(&sdTotal, sdTotal, MEDIATYPE_SD, AM_STATUS_MASK_INSTALLING | AM_STATUS_MASK_AWAITING_FINALIZATION, titleIds)) || R_FAILED(res = AM_GetPendingTitleList(&nandTotal, nandTotal, MEDIATYPE_NAND, AM_STATUS_MASK_INSTALLING | AM_STATUS_MASK_AWAITING_FINALIZATION, &titleIds[sdTotal]))) {
        error_display_res(NULL, NULL, NULL, res, "Failed to retrieve pending title list.");

        free(titleIds);
        return;
    }

    action_delete_pending_titles(info, populated, "Delete all pending titles?", titleIds, sdTotal, nandTotal);
}