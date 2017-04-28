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
    list_item* selected;
} rename_data;

static void action_rename_onresponse(ui_view* view, void* data, SwkbdButton button, const char* response) {
    rename_data* renameData = (rename_data*) data;

    if(button == SWKBD_BUTTON_CONFIRM) {
        Result res = 0;

        list_item* selected = renameData->selected;
        file_info* targetInfo = (file_info*) selected->data;

        char fileName[FILE_NAME_MAX] = {'\0'};
        util_escape_file_name(fileName, response, sizeof(fileName));

        char parentPath[FILE_PATH_MAX] = {'\0'};
        util_get_parent_path(parentPath, targetInfo->path, FILE_PATH_MAX);

        char dstPath[FILE_PATH_MAX] = {'\0'};
        if(targetInfo->attributes & FS_ATTRIBUTE_DIRECTORY) {
            snprintf(dstPath, FILE_PATH_MAX, "%s%s/", parentPath, fileName);
        } else {
            snprintf(dstPath, FILE_PATH_MAX, "%s%s", parentPath, fileName);
        }

        FS_Path* srcFsPath = util_make_path_utf8(targetInfo->path);
        if(srcFsPath != NULL) {
            FS_Path* dstFsPath = util_make_path_utf8(dstPath);
            if(dstFsPath != NULL) {
                if(targetInfo->attributes & FS_ATTRIBUTE_DIRECTORY) {
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
            if(strncmp(selected->name, "<current directory>", LIST_ITEM_NAME_MAX) != 0 && strncmp(selected->name, "<current file>", LIST_ITEM_NAME_MAX) != 0) {
                strncpy(selected->name, fileName, LIST_ITEM_NAME_MAX);
            }

            strncpy(targetInfo->name, fileName, FILE_NAME_MAX);
            strncpy(targetInfo->path, dstPath, FILE_PATH_MAX);

            linked_list_sort(renameData->items, NULL, util_compare_file_infos);

            prompt_display_notify("Success", "Renamed.", COLOR_TEXT, NULL, NULL, NULL);
        } else {
            error_display_res(NULL, NULL, res, "Failed to perform rename.");
        }
    }

    free(renameData);
}

void action_rename(linked_list* items, list_item* selected) {
    rename_data* data = (rename_data*) calloc(1, sizeof(rename_data));
    if(data == NULL) {
        error_display(NULL, NULL, "Failed to allocate rename data.");

        return;
    }

    data->items = items;
    data->selected = selected;

    kbd_display("Enter new name", ((file_info*) selected->data)->name, SWKBD_TYPE_NORMAL, 0, SWKBD_NOTEMPTY_NOTBLANK, FILE_NAME_MAX, data, action_rename_onresponse);
}