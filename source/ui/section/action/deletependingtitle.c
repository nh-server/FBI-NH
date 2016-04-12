#include <malloc.h>

#include <3ds.h>

#include "action.h"
#include "../../error.h"
#include "../../progressbar.h"
#include "../../prompt.h"
#include "../../../screen.h"

typedef struct {
    pending_title_info* info;
    bool* populated;
} delete_pending_title_data;

static void action_delete_pending_title_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    ui_draw_pending_title_info(view, ((delete_pending_title_data*) data)->info, x1, y1, x2, y2);
}

static void action_delete_pending_title_success_onresponse(ui_view* view, void* data, bool response) {
    prompt_destroy(view);
}

static void action_delete_pending_title_update(ui_view* view, void* data, float* progress, char* progressText) {
    delete_pending_title_data* deleteData = (delete_pending_title_data*) data;

    Result res = AM_DeletePendingTitle(deleteData->info->mediaType, deleteData->info->titleId);

    progressbar_destroy(view);
    ui_pop();

    if(R_FAILED(res)) {
        error_display_res(deleteData->info, ui_draw_pending_title_info, res, "Failed to delete pending title.");
    } else {
        *deleteData->populated = false;

        ui_push(prompt_create("Success", "Pending title deleted.", COLOR_TEXT, false, deleteData->info, NULL, ui_draw_pending_title_info, action_delete_pending_title_success_onresponse));
    }

    free(data);
}

static void action_delete_pending_title_onresponse(ui_view* view, void* data, bool response) {
    prompt_destroy(view);

    if(response) {
        ui_push(progressbar_create("Deleting Pending Title", "", data, action_delete_pending_title_update, action_delete_pending_title_draw_top));
    } else {
        free(data);
    }
}

void action_delete_pending_title(pending_title_info* info, bool* populated) {
    delete_pending_title_data* data = (delete_pending_title_data*) calloc(1, sizeof(delete_pending_title_data));
    data->info = info;
    data->populated = populated;

    ui_push(prompt_create("Confirmation", "Delete the selected pending title?", COLOR_TEXT, true, data, NULL, action_delete_pending_title_draw_top, action_delete_pending_title_onresponse));
}