#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <3ds.h>

#include "action.h"
#include "../task/task.h"
#include "../../error.h"
#include "../../info.h"
#include "../../kbd.h"
#include "../../list.h"
#include "../../prompt.h"
#include "../../ui.h"
#include "../../../core/linkedlist.h"
#include "../../../core/screen.h"
#include "../../../core/util.h"

typedef struct {
    linked_list* items;
    list_item* target;
} rename_data;

static void action_rename_kbd_finished(void* data, char* input) {
    rename_data* renameData = (rename_data*) data;

    if(strlen(input) == 0) {
        error_display(NULL, NULL, NULL, "No name specified.");
    }

    file_info* targetInfo = (file_info*) renameData->target->data;

    Result res = 0;

    char parentPath[FILE_PATH_MAX] = {'\0'};
    util_get_parent_path(parentPath, targetInfo->path, FILE_PATH_MAX);

    char dstPath[FILE_PATH_MAX] = {'\0'};
    snprintf(dstPath, FILE_PATH_MAX, "%s%s", parentPath, input);

    FS_Path* srcFsPath = util_make_path_utf8(targetInfo->path);
    if(srcFsPath != NULL) {
        FS_Path* dstFsPath = util_make_path_utf8(dstPath);
        if(dstFsPath != NULL) {
            if(targetInfo->isDirectory) {
                res = FSUSER_RenameDirectory(targetInfo->archive, *srcFsPath, targetInfo->archive, *dstFsPath);
            } else {
                res = FSUSER_RenameFile(targetInfo->archive, *srcFsPath, targetInfo->archive, *dstFsPath);
            }

            util_free_path_utf8(dstFsPath);
        } else {
            res = R_FBI_OUT_OF_MEMORY;
        }

        util_free_path_utf8(srcFsPath);
    } else {
        res = R_FBI_OUT_OF_MEMORY;
    }

    if(R_SUCCEEDED(res)) {
        strncpy(renameData->target->name, input, LIST_ITEM_NAME_MAX);
        strncpy(targetInfo->name, input, FILE_NAME_MAX);
        strncpy(targetInfo->path, dstPath, FILE_PATH_MAX);

        linked_list_sort(renameData->items, util_compare_file_infos);

        prompt_display("Success", "Renamed.", COLOR_TEXT, false, NULL, NULL, NULL);
    } else {
        error_display_res(NULL, NULL, NULL, res, "Failed to perform rename.");
    }

    free(data);
}

static void action_rename_kbd_canceled(void* data) {
    free(data);
}

void action_rename(linked_list* items, list_item* selected) {
    rename_data* data = (rename_data*) calloc(1, sizeof(rename_data));
    if(data == NULL) {
        error_display(NULL, NULL, NULL, "Failed to allocate rename data.");

        return;
    }

    data->items = items;
    data->target = selected;

    kbd_display("Enter New Name", data->target->name, data, NULL, action_rename_kbd_finished, action_rename_kbd_canceled);
}