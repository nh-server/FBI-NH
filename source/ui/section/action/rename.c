#include <malloc.h>
#include <stdio.h>
#include <string.h>

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

void action_rename(linked_list* items, list_item* selected) {
    file_info* targetInfo = (file_info*) selected->data;

    SwkbdState swkbd;
    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, -1);
    swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY, 0, 0);
    swkbdSetInitialText(&swkbd, targetInfo->name);
    swkbdSetHintText(&swkbd, "Enter new name");

    char textBuf[FILE_NAME_MAX];
    if(swkbdInputText(&swkbd, textBuf, sizeof(textBuf)) == SWKBD_BUTTON_CONFIRM) {
        Result res = 0;

        char parentPath[FILE_PATH_MAX] = {'\0'};
        util_get_parent_path(parentPath, targetInfo->path, FILE_PATH_MAX);

        char dstPath[FILE_PATH_MAX] = {'\0'};
        if(targetInfo->attributes & FS_ATTRIBUTE_DIRECTORY) {
            snprintf(dstPath, FILE_PATH_MAX, "%s%s/", parentPath, textBuf);
        } else {
            snprintf(dstPath, FILE_PATH_MAX, "%s%s", parentPath, textBuf);
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
                strncpy(selected->name, textBuf, LIST_ITEM_NAME_MAX);
            }

            strncpy(targetInfo->name, textBuf, FILE_NAME_MAX);
            strncpy(targetInfo->path, dstPath, FILE_PATH_MAX);

            linked_list_sort(items, NULL, util_compare_file_infos);

            prompt_display("Success", "Renamed.", COLOR_TEXT, false, NULL, NULL, NULL);
        } else {
            error_display_res(NULL, NULL, res, "Failed to perform rename.");
        }
    }
}