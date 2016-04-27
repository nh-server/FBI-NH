#include <3ds.h>

#include "action.h"
#include "../task/task.h"
#include "../../error.h"
#include "../../info.h"
#include "../../list.h"
#include "../../prompt.h"
#include "../../ui.h"
#include "../../../core/screen.h"

static void action_launch_title_update(ui_view* view, void* data, float* progress, char* text) {
    title_info* info = (title_info*) data;

    Result res = 0;

    aptOpenSession();

    if(R_SUCCEEDED(res = APT_PrepareToDoAppJump(0, info->titleId, info->mediaType))) {
        u8 buf0[0x300];
        u8 buf1[0x20];

        res = APT_DoAppJump(0x300, 0x20, buf0, buf1);
    }

    aptCloseSession();

    if(R_FAILED(res)) {
        ui_pop();
        info_destroy(view);

        error_display_res(NULL, info, ui_draw_title_info, res, "Failed to launch title.");
    }
}

static void action_launch_title_onresponse(ui_view* view, void* data, bool response) {
    if(response) {
        info_display("Launching Title", "", false, data, action_launch_title_update, ui_draw_title_info);
    }
}

void action_launch_title(linked_list* items, list_item* selected) {
    prompt_display("Confirmation", "Launch the selected title?", COLOR_TEXT, true, selected->data, NULL, ui_draw_title_info, action_launch_title_onresponse);
}