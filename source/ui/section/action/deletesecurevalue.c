#include <stdio.h>

#include <3ds.h>

#include "action.h"
#include "../../error.h"
#include "../../progressbar.h"
#include "../../prompt.h"
#include "../../../util.h"
#include "../../../screen.h"

static void action_delete_secure_value_end_onresponse(ui_view* view, void* data, bool response) {
    prompt_destroy(view);
}

static void action_delete_secure_value_update(ui_view* view, void* data, float* progress, char* progressText) {
    title_info* info = (title_info*) data;

    u64 param = ((u64) SECUREVALUE_SLOT_SD << 32) | (info->titleId & 0xFFFFFFF);
    u8 out = 0;
    Result res = FSUSER_ControlSecureSave(SECURESAVE_ACTION_DELETE, &param, sizeof(param), &out, sizeof(out));

    progressbar_destroy(view);
    ui_pop();

    if(R_FAILED(res)) {
        error_display_res(NULL, info, ui_draw_title_info, res, "Failed to delete secure value.");
    } else {
        ui_push(prompt_create("Success", "Secure value deleted.", COLOR_TEXT, false, info, NULL, ui_draw_title_info, action_delete_secure_value_end_onresponse));
    }
}

static void action_delete_secure_value_onresponse(ui_view* view, void* data, bool response) {
    prompt_destroy(view);

    if(response) {
        ui_push(progressbar_create("Deleting Secure Value", "", data, action_delete_secure_value_update, ui_draw_title_info));
    }
}

void action_delete_secure_value(title_info* info, bool* populated) {
    ui_push(prompt_create("Confirmation", "Delete the secure value of the selected title?", COLOR_TEXT, true, info, NULL, ui_draw_title_info, action_delete_secure_value_onresponse));
}