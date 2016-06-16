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
    file_info* parentDir;
} new_folder_data;

static void action_new_folder_kbd_finished(void* data, char* input) {
    new_folder_data* newFolderData = (new_folder_data*) data;

    if(strlen(input) == 0) {
        error_display(NULL, NULL, "No name specified.");
    }

    Result res = 0;

    char path[FILE_PATH_MAX] = {'\0'};
    snprintf(path, FILE_PATH_MAX, "%s%s", newFolderData->parentDir->path, input);

    FS_Path* fsPath = util_make_path_utf8(path);
    if(fsPath != NULL) {
        res = FSUSER_CreateDirectory(newFolderData->parentDir->archive, *fsPath, 0);

        util_free_path_utf8(fsPath);
    } else {
        res = R_FBI_OUT_OF_MEMORY;
    }

    if(R_SUCCEEDED(res)) {
        list_item* folderItem = NULL;
        if(R_SUCCEEDED(task_create_file_item(&folderItem, newFolderData->parentDir->archive, path))) {
            linked_list_add(newFolderData->items, folderItem);
            linked_list_sort(newFolderData->items, util_compare_file_infos);
        }

        prompt_display("Success", "Folder created.", COLOR_TEXT, false, NULL, NULL, NULL);
    } else {
        error_display_res(NULL, NULL, res, "Failed to create folder.");
    }

    free(data);
}

static void action_new_folder_kbd_canceled(void* data) {
    free(data);
}

void action_new_folder(linked_list* items, list_item* selected) {
    new_folder_data* data = (new_folder_data*) calloc(1, sizeof(new_folder_data));
    if(data == NULL) {
        error_display(NULL, NULL, "Failed to allocate new folder data.");

        return;
    }

    data->items = items;
    data->parentDir = (file_info*) selected->data;

    kbd_display("Enter Name", NULL, data, NULL, action_new_folder_kbd_finished, action_new_folder_kbd_canceled);
}