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
#include "../../../core/util.h"

static void action_import_seed_update(ui_view* view, void* data, float* progress, char* text) {
    title_info* info = (title_info*) data;

    u32 responseCode = 0;
    Result res = util_import_seed(&responseCode, info->titleId);

    ui_pop();
    info_destroy(view);

    if(R_SUCCEEDED(res)) {
        prompt_display_notify("Success", "Seed imported.", COLOR_TEXT, info, ui_draw_title_info, NULL);
    } else if(res == R_FBI_HTTP_RESPONSE_CODE) {
        error_display(NULL, NULL, "Failed to import seed.\nHTTP server returned response code %d", responseCode);
    } else {
        error_display_res(info, ui_draw_title_info, res, "Failed to import seed.");
    }
}

static void action_import_seed_onresponse(ui_view* view, void* data, u32 response) {
    if(response == PROMPT_YES) {
        info_display("Importing Seed", "", false, data, action_import_seed_update, ui_draw_title_info);
    }
}

void action_import_seed(linked_list* items, list_item* selected) {
    prompt_display_yes_no("Confirmation", "Import the seed of the selected title?", COLOR_TEXT, selected->data, ui_draw_title_info, action_import_seed_onresponse);
}