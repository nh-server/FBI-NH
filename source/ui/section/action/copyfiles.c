#include <3ds.h>

#include "action.h"
#include "clipboard.h"
#include "../../error.h"
#include "../../prompt.h"


static void action_copy_files_success_onresponse(ui_view* view, void* data, bool response) {
    prompt_destroy(view);
}

void action_copy_contents(file_info* info) {
    Result res = 0;
    if(R_FAILED(res = clipboard_set_contents(*info->archive, info->path))) {
        error_display_res(info, ui_draw_file_info, res, "Failed to copy contents to clipboard.");

        return;
    }

    ui_push(prompt_create("Success", "Content copied to clipboard.", 0xFF000000, false, info, NULL, ui_draw_file_info, action_copy_files_success_onresponse));
}