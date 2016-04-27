#include <stdio.h>

#include <3ds.h>

#include "action.h"
#include "../task/task.h"
#include "../../error.h"
#include "../../info.h"
#include "../../list.h"
#include "../../prompt.h"
#include "../../ui.h"
#include "../../../core/linkedlist.h"
#include "../../../core/screen.h"

static void action_delete_secure_value_update(ui_view* view, void* data, float* progress, char* text) {
    title_info* info = (title_info*) data;

    u64 param = ((u64) SECUREVALUE_SLOT_SD << 32) | (info->titleId & 0xFFFFFFF);
    u8 out = 0;
    Result res = FSUSER_ControlSecureSave(SECURESAVE_ACTION_DELETE, &param, sizeof(param), &out, sizeof(out));

    ui_pop();
    info_destroy(view);

    if(R_FAILED(res)) {
        error_display_res(NULL, info, ui_draw_title_info, res, "Failed to delete secure value.");
    } else {
        prompt_display("Success", "Secure value deleted.", COLOR_TEXT, false, info, NULL, ui_draw_title_info, NULL);
    }
}

static void action_delete_secure_value_onresponse(ui_view* view, void* data, bool response) {
    if(response) {
        info_display("Deleting Secure Value", "", false, data, action_delete_secure_value_update, ui_draw_title_info);
    }
}

void action_delete_secure_value(linked_list* items, list_item* selected) {
    prompt_display("Confirmation", "Delete the secure value of the selected title?", COLOR_TEXT, true, selected->data, NULL, ui_draw_title_info, action_delete_secure_value_onresponse);
}