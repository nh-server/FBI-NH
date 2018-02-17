#include <malloc.h>
#include <stdio.h>

#include <3ds.h>

#include "action.h"
#include "../resources.h"
#include "../task/uitask.h"
#include "../../core/core.h"

typedef struct {
    linked_list* items;
    list_item* selected;
} new_folder_data;

static void action_new_folder_onresponse(ui_view* view, void* data, SwkbdButton button, const char* response) {
    new_folder_data* newFolderData = (new_folder_data*) data;

    if(button == SWKBD_BUTTON_CONFIRM) {
        Result res = 0;

        file_info* parentDir = (file_info*) newFolderData->selected->data;

        char fileName[FILE_NAME_MAX] = {'\0'};
        string_escape_file_name(fileName, response, sizeof(fileName));

        char path[FILE_PATH_MAX] = {'\0'};
        snprintf(path, FILE_PATH_MAX, "%s%s", parentDir->path, fileName);

        FS_Path* fsPath = fs_make_path_utf8(path);
        if(fsPath != NULL) {
            res = FSUSER_CreateDirectory(parentDir->archive, *fsPath, FS_ATTRIBUTE_DIRECTORY);

            fs_free_path_utf8(fsPath);
        } else {
            res = R_APP_OUT_OF_MEMORY;
        }

        if(R_SUCCEEDED(res)) {
            list_item* folderItem = NULL;
            if(R_SUCCEEDED(task_create_file_item(&folderItem, parentDir->archive, path, FS_ATTRIBUTE_DIRECTORY, true))) {
                linked_list_add(newFolderData->items, folderItem);
                linked_list_sort(newFolderData->items, NULL, task_compare_files);
            }

            prompt_display_notify("Success", "Folder created.", COLOR_TEXT, NULL, NULL, NULL);
        } else {
            error_display_res(NULL, NULL, res, "Failed to create folder.");
        }
    }

    free(newFolderData);
}

void action_new_folder(linked_list* items, list_item* selected) {
    new_folder_data* data = (new_folder_data*) calloc(1, sizeof(new_folder_data));
    if(data == NULL) {
        error_display(NULL, NULL, "Failed to allocate new folder data.");

        return;
    }

    data->items = items;
    data->selected = selected;

    kbd_display("Enter folder name", "", SWKBD_TYPE_NORMAL, 0, SWKBD_NOTEMPTY_NOTBLANK, FILE_NAME_MAX, data, action_new_folder_onresponse);
}