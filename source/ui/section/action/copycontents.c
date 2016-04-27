#include <3ds.h>

#include "action.h"
#include "clipboard.h"
#include "../task/task.h"
#include "../../error.h"
#include "../../list.h"
#include "../../prompt.h"
#include "../../ui.h"
#include "../../../core/screen.h"

static void action_copy_contents_internal(linked_list* items, list_item* selected, file_info* target, const char* message, bool contentsOnly) {
    Result res = 0;
    if(R_FAILED(res = clipboard_set_contents(*target->archive, target->path, contentsOnly))) {
        error_display_res(NULL, target, ui_draw_file_info, res, "Failed to copy to clipboard.");

        return;
    }

    prompt_display("Success", message, COLOR_TEXT, false, target, NULL, ui_draw_file_info, NULL);
}

void action_copy_content(linked_list* items, list_item* selected, file_info* target) {
    action_copy_contents_internal(items, selected, target, "Selected content copied to clipboard.", false);
}

void action_copy_contents(linked_list* items, list_item* selected, file_info* target) {
    action_copy_contents_internal(items, selected, target, "Directory contents copied to clipboard.", true);
}