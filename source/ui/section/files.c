#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <3ds.h>

#include "section.h"
#include "action/action.h"
#include "task/task.h"
#include "../error.h"
#include "../list.h"
#include "../ui.h"
#include "../../core/linkedlist.h"
#include "../../core/screen.h"
#include "../../core/util.h"

static list_item copy = {"Copy", COLOR_TEXT, action_copy_content};
static list_item paste = {"Paste", COLOR_TEXT, action_paste_contents};

static list_item delete_file = {"Delete", COLOR_TEXT, action_delete_contents};

static list_item install_cia = {"Install CIA", COLOR_TEXT, action_install_cia};
static list_item install_and_delete_cia = {"Install and delete CIA", COLOR_TEXT, action_install_cia_delete};

static list_item install_ticket = {"Install ticket", COLOR_TEXT, action_install_ticket};
static list_item install_and_delete_ticket = {"Install and delete ticket", COLOR_TEXT, action_install_ticket_delete};

static list_item delete_dir = {"Delete", COLOR_TEXT, action_delete_dir};
static list_item delete_all_contents = {"Delete all contents", COLOR_TEXT, action_delete_dir_contents};
static list_item copy_all_contents = {"Copy all contents", COLOR_TEXT, action_copy_contents};

static list_item install_all_cias = {"Install all CIAs", COLOR_TEXT, action_install_cias};
static list_item install_and_delete_all_cias = {"Install and delete all CIAs", COLOR_TEXT, action_install_cias_delete};
static list_item delete_all_cias = {"Delete all CIAs", COLOR_TEXT, action_delete_dir_cias};

static list_item install_all_tickets = {"Install all tickets", COLOR_TEXT, action_install_tickets};
static list_item install_and_delete_all_tickets = {"Install and delete all tickets", COLOR_TEXT, action_install_tickets_delete};
static list_item delete_all_tickets = {"Delete all tickets", COLOR_TEXT, action_delete_dir_tickets};

typedef struct {
    Handle cancelEvent;
    bool populated;

    FS_Archive archive;
    void* archivePath;

    file_info currDir;
    file_info parentDir;
} files_data;

typedef struct {
    linked_list* items;
    list_item* selected;

    file_info* target;
} files_action_data;

static void files_action_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    ui_draw_file_info(view, ((files_action_data*) data)->target, x1, y1, x2, y2);
}

static void files_action_update(ui_view* view, void* data, linked_list* items, list_item* selected, bool selectedTouched) {
    files_action_data* actionData = (files_action_data*) data;

    if(hidKeysDown() & KEY_B) {
        ui_pop();
        list_destroy(view);

        free(data);

        return;
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        void(*action)(linked_list*, list_item*, file_info*) = (void(*)(linked_list*, list_item*, file_info*)) selected->data;

        ui_pop();
        list_destroy(view);

        action(actionData->items, actionData->selected, actionData->target);

        free(data);

        return;
    }

    if(linked_list_size(items) == 0) {
        if(actionData->target->isDirectory) {
            if(actionData->target->containsCias) {
                linked_list_add(items, &install_all_cias);
                linked_list_add(items, &install_and_delete_all_cias);
                linked_list_add(items, &delete_all_cias);
            }

            if(actionData->target->containsTickets) {
                linked_list_add(items, &install_all_tickets);
                linked_list_add(items, &install_and_delete_all_tickets);
                linked_list_add(items, &delete_all_tickets);
            }

            linked_list_add(items, &delete_all_contents);
            linked_list_add(items, &copy_all_contents);

            linked_list_add(items, &delete_dir);
        } else {
            if(actionData->target->isCia) {
                linked_list_add(items, &install_cia);
                linked_list_add(items, &install_and_delete_cia);
            }

            if(actionData->target->isTicket) {
                linked_list_add(items, &install_ticket);
                linked_list_add(items, &install_and_delete_ticket);
            }

            linked_list_add(items, &delete_file);
        }

        linked_list_add(items, &copy);
        linked_list_add(items, &paste);
    }
}

static void files_action_open(linked_list* items, list_item* selected, file_info* target) {
    files_action_data* data = (files_action_data*) calloc(1, sizeof(files_action_data));
    if(data == NULL) {
        error_display(NULL, NULL, NULL, "Failed to allocate files action data.");

        return;
    }

    data->items = items;
    data->selected = selected;

    data->target = target;

    list_display(target->isDirectory ? "Directory Action" : "File Action", "A: Select, B: Return", data, files_action_update, files_action_draw_top);
}

static void files_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    if(selected != NULL && selected->data != NULL) {
        ui_draw_file_info(view, selected->data, x1, y1, x2, y2);
    }
}

static void files_repopulate(files_data* listData, linked_list* items) {
    if(listData->cancelEvent != 0) {
        svcSignalEvent(listData->cancelEvent);
        while(svcWaitSynchronization(listData->cancelEvent, 0) == 0) {
            svcSleepThread(1000000);
        }

        listData->cancelEvent = 0;
    }

    while(!util_is_dir(&listData->archive, listData->currDir.path)) {
        char parentPath[FILE_PATH_MAX];

        util_get_parent_path(parentPath, listData->currDir.path, FILE_PATH_MAX);
        strncpy(listData->currDir.path, parentPath, FILE_PATH_MAX);
        util_get_path_file(listData->currDir.name, listData->currDir.path, FILE_NAME_MAX);

        util_get_parent_path(parentPath, listData->currDir.path, FILE_PATH_MAX);
        strncpy(listData->parentDir.path, parentPath, FILE_PATH_MAX);
        util_get_path_file(listData->parentDir.name, listData->parentDir.path, FILE_NAME_MAX);
    }

    listData->cancelEvent = task_populate_files(items, &listData->currDir);
    listData->populated = true;
}

static void files_navigate(files_data* listData, linked_list* items, const char* path) {
    strncpy(listData->currDir.path, path, FILE_PATH_MAX);
    util_get_path_file(listData->currDir.name, listData->currDir.path, FILE_NAME_MAX);

    char parentPath[FILE_PATH_MAX];
    util_get_parent_path(parentPath, listData->currDir.path, FILE_PATH_MAX);
    strncpy(listData->parentDir.path, parentPath, FILE_PATH_MAX);
    util_get_path_file(listData->parentDir.name, listData->parentDir.path, FILE_NAME_MAX);

    files_repopulate(listData, items);
}

static void files_update(ui_view* view, void* data, linked_list* items, list_item* selected, bool selectedTouched) {
    files_data* listData = (files_data*) data;

    if(hidKeysDown() & KEY_B) {
        if(strcmp(listData->currDir.path, "/") == 0) {
            if(listData->archive.handle != 0) {
                FSUSER_CloseArchive(&listData->archive);
                listData->archive.handle = 0;
            }

            if(listData->archivePath != NULL) {
                free(listData->archivePath);
                listData->archivePath = NULL;
            }

            if(listData->cancelEvent != 0) {
                svcSignalEvent(listData->cancelEvent);
                while(svcWaitSynchronization(listData->cancelEvent, 0) == 0) {
                    svcSleepThread(1000000);
                }

                listData->cancelEvent = 0;
            }

            ui_pop();

            task_clear_files(items);
            list_destroy(view);

            free(listData);
            return;
        } else {
            files_navigate(listData, items, listData->parentDir.path);
        }
    }

    if(hidKeysDown() & KEY_Y) {
        files_action_open(items, selected, &listData->currDir);
        return;
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        file_info* fileInfo = (file_info*) selected->data;

        if(util_is_dir(&listData->archive, fileInfo->path)) {
            files_navigate(listData, items, fileInfo->path);
        } else {
            files_action_open(items, selected, fileInfo);
            return;
        }
    }

    if(!listData->populated || (hidKeysDown() & KEY_X)) {
        files_repopulate(listData, items);
    }
}

void files_open(FS_Archive archive) {
    files_data* data = (files_data*) calloc(1, sizeof(files_data));
    if(data == NULL) {
        error_display(NULL, NULL, NULL, "Failed to allocate files data.");

        return;
    }

    data->archive = archive;

    if(data->archive.lowPath.size > 0) {
        data->archivePath = calloc(1,  data->archive.lowPath.size);
        if(data->archivePath == NULL) {
            error_display(NULL, NULL, NULL, "Failed to allocate files archive.");

            free(data);
            return;
        }

        memcpy(data->archivePath,  data->archive.lowPath.data,  data->archive.lowPath.size);
        data->archive.lowPath.data = data->archivePath;
    }

    data->archive.handle = 0;

    Result res = 0;
    if(R_FAILED(res = FSUSER_OpenArchive(&data->archive))) {
        error_display_res(NULL, NULL, NULL, res, "Failed to open file listing archive.");

        if(data->archivePath != NULL) {
            free(data->archivePath);
        }

        free(data);
        return;
    }

    data->currDir.archive = &data->archive;
    snprintf(data->currDir.path, FILE_PATH_MAX, "/");
    util_get_path_file(data->currDir.name, data->currDir.path, FILE_NAME_MAX);
    data->currDir.isDirectory = true;
    data->currDir.containsCias = false;
    data->currDir.size = 0;
    data->currDir.isCia = false;

    memcpy(&data->parentDir, &data->currDir, sizeof(data->parentDir));

    list_display("Files", "A: Select, B: Back, X: Refresh, Y: Directory Action", data, files_update, files_draw_top);
}

void files_open_sd() {
    FS_Archive sdmcArchive = {ARCHIVE_SDMC, {PATH_BINARY, 0, (void*) ""}};
    files_open(sdmcArchive);
}

void files_open_ctr_nand() {
    FS_Archive ctrNandArchive = {ARCHIVE_NAND_CTR_FS, fsMakePath(PATH_EMPTY, "")};
    files_open(ctrNandArchive);
}

void files_open_twl_nand() {
    FS_Archive twlNandArchive = {ARCHIVE_NAND_TWL_FS, fsMakePath(PATH_EMPTY, "")};
    files_open(twlNandArchive);
}

void files_open_twl_photo() {
    FS_Archive twlPhotoArchive = {ARCHIVE_TWL_PHOTO, fsMakePath(PATH_EMPTY, "")};
    files_open(twlPhotoArchive);
}

void files_open_twl_sound() {
    FS_Archive twlSoundArchive = {ARCHIVE_TWL_SOUND, {PATH_EMPTY, 0, ""}};
    files_open(twlSoundArchive);
}