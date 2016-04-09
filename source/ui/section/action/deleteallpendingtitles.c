#include <3ds.h>

#include "action.h"
#include "../../error.h"
#include "../../progressbar.h"
#include "../../prompt.h"

static void action_delete_all_pending_titles_success_onresponse(ui_view* view, void* data, bool response) {
    prompt_destroy(view);
}

static void action_delete_all_pending_titles_update(ui_view* view, void* data, float* progress, char* progressText) {
    Result res = AM_DeleteAllPendingTitles(MEDIATYPE_NAND);
    if(R_SUCCEEDED(res)) {
        res = AM_DeleteAllPendingTitles(MEDIATYPE_SD);
    }

    if(R_FAILED(res)) {
        progressbar_destroy(view);
        ui_pop();

        error_display_res(NULL, NULL, res, "Failed to delete pending titles.");

        return;
    } else {
        *(bool*) data = false;
    }

    progressbar_destroy(view);
    ui_pop();

    ui_push(prompt_create("Success", "Pending titles deleted.", 0xFF000000, false, NULL, NULL, NULL, action_delete_all_pending_titles_success_onresponse));
}

static void action_delete_all_pending_titles_onresponse(ui_view* view, void* data, bool response) {
    prompt_destroy(view);

    if(response) {
        ui_push(progressbar_create("Deleting Pending Titles", "", data, action_delete_all_pending_titles_update, NULL));
    }
}

void action_delete_all_pending_titles(pending_title_info* info, bool* populated) {
    ui_push(prompt_create("Confirmation", "Delete all pending titles?", 0xFF000000, true, populated, NULL, NULL, action_delete_all_pending_titles_onresponse));
}