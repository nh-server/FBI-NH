#include <3ds.h>

#include "action.h"
#include "../../error.h"
#include "../../progressbar.h"
#include "../../prompt.h"

static void action_launch_title_update(ui_view* view, void* data, float* progress, char* progressText) {
    title_info* info = (title_info*) data;

    u8 buf0[0x300];
    u8 buf1[0x20];

    aptOpenSession();

    Result res = APT_PrepareToDoAppJump(0, info->titleId, info->mediaType);
    if(R_SUCCEEDED(res)) {
        res = APT_DoAppJump(0x300, 0x20, buf0, buf1);
    }

    aptCloseSession();

    progressbar_destroy(view);

    if(R_SUCCEEDED(res)) {
        while(ui_peek() != NULL) {
            ui_pop();
        }
    } else {
        progressbar_destroy(view);
        ui_pop();

        error_display_res(info, ui_draw_title_info, res, "Failed to launch title.");
    }
}

static void action_launch_title_onresponse(ui_view* view, void* data, bool response) {
    if(response) {
        prompt_destroy(view);

        ui_push(progressbar_create("Launching Title", "", data, action_launch_title_update, ui_draw_title_info));
    }
}

void action_launch_title(title_info* info) {
    ui_push(prompt_create("Confirmation", "Launch the selected title?", 0xFF000000, true, info, NULL, ui_draw_title_info, action_launch_title_onresponse));
}