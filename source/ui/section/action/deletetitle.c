#include <3ds.h>

#include "action.h"
#include "../../error.h"
#include "../../progressbar.h"
#include "../../prompt.h"

static void action_delete_title_success_onresponse(ui_view* view, void* data, bool response) {
    task_refresh_pending_titles();

    prompt_destroy(view);
}

static void action_delete_title_update(ui_view* view, void* data, float* progress, char* progressText) {
    title_info* info = (title_info*) data;

    Result res = AM_DeleteTitle(info->mediaType, info->titleId);
    if(R_FAILED(res)) {
        progressbar_destroy(view);
        ui_pop();

        error_display_res(info, ui_draw_title_info, res, "Failed to delete title.");

        return;
    }

    progressbar_destroy(view);
    ui_pop();

    ui_push(prompt_create("Success", "Title deleted.", 0xFF000000, false, info, NULL, ui_draw_title_info, action_delete_title_success_onresponse));
}

static void action_delete_title_onresponse(ui_view* view, void* data, bool response) {
    if(response) {
        prompt_destroy(view);

        ui_push(progressbar_create("Deleting Title", "", data, action_delete_title_update, ui_draw_title_info));
    }
}

void action_delete_title(title_info* info) {
    ui_push(prompt_create("Confirmation", "Delete the selected title?", 0xFF000000, true, info, NULL, ui_draw_title_info, action_delete_title_onresponse));
}