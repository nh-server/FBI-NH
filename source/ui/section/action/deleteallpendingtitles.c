#include <3ds.h>

#include "action.h"
#include "../../error.h"
#include "../../progressbar.h"
#include "../../prompt.h"

static void action_delete_all_pending_titles_success_onresponse(ui_view* view, void* data, bool response) {
    task_refresh_pending_titles();

    prompt_destroy(view);
}

static void action_delete_all_pending_titles_update(ui_view* view, void* data, float* progress, char* progressText) {
    pending_title_info* info = (pending_title_info*) data;

    Result res = AM_DeleteAllPendingTitles(MEDIATYPE_NAND);
    if(R_SUCCEEDED(res)) {
        res = AM_DeleteAllPendingTitles(MEDIATYPE_SD);
    }

    if(R_FAILED(res)) {
        progressbar_destroy(view);
        ui_pop();

        error_display_res(info, ui_draw_pending_title_info, res, "Failed to delete pending titles.");

        return;
    }

    progressbar_destroy(view);
    ui_pop();

    ui_push(prompt_create("Success", "Pending titles deleted.", 0xFF000000, false, info, NULL, NULL, action_delete_all_pending_titles_success_onresponse));
}

static void action_delete_all_pending_titles_onresponse(ui_view* view, void* data, bool response) {
    if(response) {
        prompt_destroy(view);

        ui_push(progressbar_create("Deleting Pending Titles", "", data, action_delete_all_pending_titles_update, NULL));
    }
}

void action_delete_all_pending_titles(pending_title_info* info) {
    ui_push(prompt_create("Confirmation", "Delete all pending titles?", 0xFF000000, true, NULL, NULL, NULL, action_delete_all_pending_titles_onresponse));
}