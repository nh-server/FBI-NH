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

void action_new_folder(linked_list* items, list_item* selected) {
    SwkbdState swkbd;
    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, -1);
    swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY, 0, 0);
    swkbdSetHintText(&swkbd, "Enter folder name");

    char textBuf[FILE_NAME_MAX];
    if(swkbdInputText(&swkbd, textBuf, sizeof(textBuf)) == SWKBD_BUTTON_CONFIRM) {
        Result res = 0;

        file_info* parentDir = (file_info*) selected->data;

        char path[FILE_PATH_MAX] = {'\0'};
        snprintf(path, FILE_PATH_MAX, "%s%s", parentDir->path, textBuf);

        FS_Path* fsPath = util_make_path_utf8(path);
        if(fsPath != NULL) {
            res = FSUSER_CreateDirectory(parentDir->archive, *fsPath, FS_ATTRIBUTE_DIRECTORY);

            util_free_path_utf8(fsPath);
        } else {
            res = R_FBI_OUT_OF_MEMORY;
        }

        if(R_SUCCEEDED(res)) {
            list_item* folderItem = NULL;
            if(R_SUCCEEDED(task_create_file_item(&folderItem, parentDir->archive, path, FS_ATTRIBUTE_DIRECTORY))) {
                linked_list_add(items, folderItem);
                linked_list_sort(items, NULL, util_compare_file_infos);
            }

            prompt_display("Success", "Folder created.", COLOR_TEXT, false, NULL, NULL, NULL);
        } else {
            error_display_res(NULL, NULL, res, "Failed to create folder.");
        }
    }
}