#include <3ds.h>

#include "action.h"
#include "clipboard.h"
#include "../../error.h"
#include "../../prompt.h"
#include "../../../screen.h"

void action_copy_contents(file_info* info, bool* populated) {
    Result res = 0;
    if(R_FAILED(res = clipboard_set_contents(*info->archive, info->path))) {
        error_display_res(NULL, info, ui_draw_file_info, res, "Failed to copy contents to clipboard.");

        return;
    }

    prompt_display("Success", "Content copied to clipboard.", COLOR_TEXT, false, info, NULL, ui_draw_file_info, NULL);
}