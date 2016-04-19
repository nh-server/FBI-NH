#include <malloc.h>
#include <stdio.h>

#include <3ds.h>

#include "action.h"
#include "../../error.h"
#include "../../progressbar.h"
#include "../../prompt.h"
#include "../../../screen.h"
#include "../task/task.h"

typedef struct {
    pending_title_info* info;
    bool* populated;

    u64* titleIds;
    u32 processed;
    u32 total;

    u32 sdTotal;
    u32 nandTotal;
} delete_pending_titles_data;

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

static void action_delete_pending_titles_success_onresponse(ui_view* view, void* data, bool response) {
    prompt_destroy(view);

    action_delete_pending_titles_free_data((delete_pending_titles_data*) data);
}

static void action_delete_pending_titles_update(ui_view* view, void* data, float* progress, char* progressText) {
    delete_pending_titles_data* deleteData = (delete_pending_titles_data*) data;

    if(deleteData->processed >= deleteData->total) {
        progressbar_destroy(view);
        ui_pop();

        ui_push(prompt_create("Success", "Pending title(s) deleted.", COLOR_TEXT, false, data, NULL, action_delete_pending_titles_draw_top, action_delete_pending_titles_success_onresponse));
        return;
    } else {
        Result res = AM_DeletePendingTitle(deleteData->processed >= deleteData->sdTotal ? MEDIATYPE_NAND : MEDIATYPE_SD, deleteData->titleIds[deleteData->processed]);
        if(R_FAILED(res)) {
            progressbar_destroy(view);
            ui_pop();

            error_display_res(NULL, deleteData->info, deleteData->info != NULL ? ui_draw_pending_title_info : NULL, res, "Failed to delete pending title(s).");

            action_delete_pending_titles_free_data(deleteData);

            return;
        } else {
            *deleteData->populated = false;

            deleteData->processed++;
        }
    }

    *progress = deleteData->total > 0 ? (float) deleteData->processed / (float) deleteData->total : 0;
    snprintf(progressText, PROGRESS_TEXT_MAX, "%lu / %lu", deleteData->processed, deleteData->total);
}

static void action_delete_pending_titles_onresponse(ui_view* view, void* data, bool response) {
    prompt_destroy(view);

    if(response) {
        ui_push(progressbar_create("Deleting Pending Title(s)", "", data, action_delete_pending_titles_update, action_delete_pending_titles_draw_top));
    } else {
        action_delete_pending_titles_free_data((delete_pending_titles_data*) data);
    }
}

void action_delete_pending_title(pending_title_info* info, bool* populated) {
    delete_pending_titles_data* data = (delete_pending_titles_data*) calloc(1, sizeof(delete_pending_titles_data));
    data->info = info;
    data->populated = populated;
    data->titleIds = (u64*) calloc(1, sizeof(u64));
    data->titleIds[0] = info->titleId;
    data->processed = 0;
    data->total = 1;
    data->sdTotal = info->mediaType == MEDIATYPE_SD ? 1 : 0;
    data->nandTotal = info->mediaType == MEDIATYPE_NAND ? 1 : 0;

    ui_push(prompt_create("Confirmation", "Delete the selected pending title?", COLOR_TEXT, true, data, NULL, action_delete_pending_titles_draw_top, action_delete_pending_titles_onresponse));
}

void action_delete_all_pending_titles(pending_title_info* info, bool* populated) {
    delete_pending_titles_data* data = (delete_pending_titles_data*) calloc(1, sizeof(delete_pending_titles_data));
    data->info = NULL;
    data->populated = populated;
    data->titleIds = NULL;
    data->processed = 0;
    data->total = 0;
    data->sdTotal = 0;
    data->nandTotal = 0;

    Result res = 0;
    if(R_FAILED(res = AM_GetPendingTitleCount(&data->sdTotal, MEDIATYPE_SD, AM_STATUS_MASK_INSTALLING | AM_STATUS_MASK_AWAITING_FINALIZATION)) || R_FAILED(res = AM_GetPendingTitleCount(&data->nandTotal, MEDIATYPE_NAND, AM_STATUS_MASK_INSTALLING | AM_STATUS_MASK_AWAITING_FINALIZATION))) {
        error_display_res(NULL, NULL, NULL, res, "Failed to retrieve pending title count.");

        free(data);
        return;
    }

    data->total = data->sdTotal + data->nandTotal;
    data->titleIds = (u64*) calloc(data->total, sizeof(u64));
    if(R_FAILED(res = AM_GetPendingTitleList(&data->sdTotal, data->sdTotal, MEDIATYPE_SD, AM_STATUS_MASK_INSTALLING | AM_STATUS_MASK_AWAITING_FINALIZATION, data->titleIds)) || R_FAILED(res = AM_GetPendingTitleList(&data->nandTotal, data->nandTotal, MEDIATYPE_NAND, AM_STATUS_MASK_INSTALLING | AM_STATUS_MASK_AWAITING_FINALIZATION, &data->titleIds[data->sdTotal]))) {
        error_display_res(NULL, NULL, NULL, res, "Failed to retrieve pending title list.");

        free(data->titleIds);
        free(data);
        return;
    }

    ui_push(prompt_create("Confirmation", "Delete all pending titles?", COLOR_TEXT, true, data, NULL, action_delete_pending_titles_draw_top, action_delete_pending_titles_onresponse));
}