#include <3ds.h>

#include "action.h"
#include "../resources.h"
#include "../task/uitask.h"
#include "../../core/core.h"

static void action_launch_title_update(ui_view* view, void* data, float* progress, char* text) {
    title_info* info = (title_info*) data;

    Result res = 0;

    if(R_SUCCEEDED(res = APT_PrepareToDoApplicationJump(0, info->titleId, info->mediaType))) {
        u8 param[0x300];
        u8 hmac[0x20];

        res = APT_DoApplicationJump(param, sizeof(param), hmac);
    }

    if(R_FAILED(res)) {
        ui_pop();
        info_destroy(view);

        error_display_res(info, task_draw_title_info, res, "Failed to launch title.");
    }
}

static void action_launch_title_onresponse(ui_view* view, void* data, u32 response) {
    if(response == PROMPT_YES) {
        info_display("Launching Title", "", false, data, action_launch_title_update, task_draw_title_info);
    }
}

void action_launch_title(linked_list* items, list_item* selected) {
    prompt_display_yes_no("Confirmation", "Launch the selected title?", COLOR_TEXT, selected->data, task_draw_title_info, action_launch_title_onresponse);
}