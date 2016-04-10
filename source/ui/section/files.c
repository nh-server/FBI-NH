#include <dirent.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <3ds.h>

#include "action/action.h"
#include "task/task.h"
#include "../../util.h"
#include "../error.h"
#include "section.h"

#define FILES_MAX 1024

typedef struct {
    list_item items[FILES_MAX];
    u32 count;
    Handle cancelEvent;
    bool populated;

    FS_Archive archive;
    void* archivePath;
    char path[PATH_MAX];
} files_data;

#define FILES_ACTION_COUNT 3

static u32 files_action_count = FILES_ACTION_COUNT;
static list_item files_action_items[FILES_ACTION_COUNT] = {
        {"Delete", 0xFF000000, action_delete_contents},
        {"Copy", 0xFF000000, action_copy_contents},
        {"Paste", 0xFF000000, action_paste_contents},
};

#define CIA_FILES_ACTION_COUNT 5

static u32 cia_files_action_count = CIA_FILES_ACTION_COUNT;
static list_item cia_files_action_items[CIA_FILES_ACTION_COUNT] = {
        {"Install CIA to SD", 0xFF000000, action_install_cias_sd},
        {"Install CIA to NAND", 0xFF000000, action_install_cias_nand},
        {"Delete", 0xFF000000, action_delete_contents},
        {"Copy", 0xFF000000, action_copy_contents},
        {"Paste", 0xFF000000, action_paste_contents},
};

#define DIRECTORIES_ACTION_COUNT 4

static u32 directories_action_count = DIRECTORIES_ACTION_COUNT;
static list_item directories_action_items[DIRECTORIES_ACTION_COUNT] = {
        {"Delete all contents", 0xFF000000, action_delete_dir_contents},
        {"Delete", 0xFF000000, action_delete_contents},
        {"Copy", 0xFF000000, action_copy_contents},
        {"Paste", 0xFF000000, action_paste_contents},
};

#define CIA_DIRECTORIES_ACTION_COUNT 7

static u32 cia_directories_action_count = CIA_DIRECTORIES_ACTION_COUNT;
static list_item cia_directories_action_items[CIA_DIRECTORIES_ACTION_COUNT] = {
        {"Install all CIAs to SD", 0xFF000000, action_install_cias_sd},
        {"Install all CIAs to NAND", 0xFF000000, action_install_cias_nand},
        {"Delete all CIAs", 0xFF000000, action_delete_dir_cias},
        {"Delete all contents", 0xFF000000, action_delete_dir_contents},
        {"Delete", 0xFF000000, action_delete_contents},
        {"Copy", 0xFF000000, action_copy_contents},
        {"Paste", 0xFF000000, action_paste_contents},
};

typedef struct {
    file_info* info;
    bool* populated;
} files_action_data;

static void files_action_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    ui_draw_file_info(view, ((files_action_data*) data)->info, x1, y1, x2, y2);
}

static void files_action_update(ui_view* view, void* data, list_item** items, u32** itemCount, list_item* selected, bool selectedTouched) {
    files_action_data* actionData = (files_action_data*) data;

    if(hidKeysDown() & KEY_B) {
        list_destroy(view);
        ui_pop();

        free(data);

        return;
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        void(*action)(file_info*, bool*) = (void(*)(file_info*, bool*)) selected->data;

        list_destroy(view);
        ui_pop();

        action(actionData->info, actionData->populated);

        free(data);

        return;
    }

    if(actionData->info->isDirectory) {
        if(actionData->info->containsCias) {
            if(*itemCount != &cia_directories_action_count || *items != cia_directories_action_items) {
                *itemCount = &cia_directories_action_count;
                *items = cia_directories_action_items;
            }
        } else {
            if(*itemCount != &directories_action_count || *items != directories_action_items) {
                *itemCount = &directories_action_count;
                *items = directories_action_items;
            }
        }
    } else {
        if(actionData->info->isCia) {
            if(*itemCount != &cia_files_action_count || *items != cia_files_action_items) {
                *itemCount = &cia_files_action_count;
                *items = cia_files_action_items;
            }
        } else {
            if(*itemCount != &files_action_count || *items != files_action_items) {
                *itemCount = &files_action_count;
                *items = files_action_items;
            }
        }
    }
}

static ui_view* files_action_create(file_info* info, bool* populated) {
    files_action_data* data = (files_action_data*) calloc(1, sizeof(files_action_data));
    data->info = info;
    data->populated = populated;

    return list_create(info->isDirectory ? "Directory Action" : "File Action", "A: Select, B: Return", data, files_action_update, files_action_draw_top);
}

static void files_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    if(selected != NULL && selected->data != NULL) {
        ui_draw_file_info(view, selected->data, x1, y1, x2, y2);
    }
}

static void files_repopulate(files_data* listData) {
    if(listData->cancelEvent != 0) {
        svcSignalEvent(listData->cancelEvent);
        while(svcWaitSynchronization(listData->cancelEvent, 0) == 0) {
            svcSleepThread(1000000);
        }

        listData->cancelEvent = 0;
    }

    if(!util_is_dir(&listData->archive, listData->path)) {
        char parentPath[PATH_MAX];
        util_get_parent_path(parentPath, listData->path, PATH_MAX);

        strncpy(listData->path, parentPath, PATH_MAX);
    }

    listData->cancelEvent = task_populate_files(listData->items, &listData->count, FILES_MAX, &listData->archive, listData->path);
    listData->populated = true;
}

static void files_update(ui_view* view, void* data, list_item** items, u32** itemCount, list_item* selected, bool selectedTouched) {
    files_data* listData = (files_data*) data;

    if(hidKeysDown() & KEY_B) {
        if(strcmp(listData->path, "/") == 0) {
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
            free(listData);
            list_destroy(view);
            return;
        } else if(*items != NULL && *itemCount != NULL) {
            for(u32 i = 0; i < **itemCount; i++) {
                char* name = (*items)[i].name;
                file_info* fileInfo = (*items)[i].data;
                if(fileInfo != NULL && strcmp(name, "..") == 0) {
                    strncpy(listData->path, fileInfo->path, PATH_MAX);
                    files_repopulate(listData);
                    break;
                }
            }
        }
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        file_info* fileInfo = (file_info*) selected->data;

        if(strcmp(selected->name, ".") == 0) {
            ui_push(files_action_create(fileInfo, &listData->populated));
            return;
        } else if(strcmp(selected->name, "..") == 0) {
            strncpy(listData->path, fileInfo->path, PATH_MAX);
            files_repopulate(listData);
        } else {
            if(util_is_dir(&listData->archive, fileInfo->path)) {
                strncpy(listData->path, fileInfo->path, PATH_MAX);
                files_repopulate(listData);
            } else {
                ui_push(files_action_create(fileInfo, &listData->populated));
                return;
            }
        }
    }

    if(!listData->populated || (hidKeysDown() & KEY_X)) {
        files_repopulate(listData);
    }

    if(*itemCount != &listData->count || *items != listData->items) {
        *itemCount = &listData->count;
        *items = listData->items;
    }
}

void files_open(FS_Archive archive) {
    files_data* data = (files_data*) calloc(1, sizeof(files_data));
    data->archive = archive;
    snprintf(data->path, PATH_MAX, "/");

    if(data->archive.lowPath.size > 0) {
        data->archivePath = calloc(1,  data->archive.lowPath.size);
        memcpy(data->archivePath,  data->archive.lowPath.data,  data->archive.lowPath.size);
        data->archive.lowPath.data = data->archivePath;
    }

    data->archive.handle = 0;

    Result res = 0;
    if(R_FAILED(res = FSUSER_OpenArchive(&data->archive))) {
        error_display_res(NULL, NULL, res, "Failed to open file listing archive.");

        if(data->archivePath != NULL) {
            free(data->archivePath);
        }

        free(data);
        return;
    }

    ui_push(list_create("Files", "A: Select, B: Back/Return, X: Refresh", data, files_update, files_draw_top));
}

void files_open_sd() {
    FS_Archive sdmcArchive = {ARCHIVE_SDMC, {PATH_BINARY, 0, (void*) ""}};
    files_open(sdmcArchive);
}

void files_open_ctr_nand() {
    FS_Archive sdmcArchive = {ARCHIVE_NAND_CTR_FS, fsMakePath(PATH_EMPTY, "")};
    files_open(sdmcArchive);
}

void files_open_twl_nand() {
    FS_Archive sdmcArchive = {ARCHIVE_NAND_TWL_FS, fsMakePath(PATH_EMPTY, "")};
    files_open(sdmcArchive);
}

void files_open_twl_photo() {
    FS_Archive sdmcArchive = {ARCHIVE_TWL_PHOTO, fsMakePath(PATH_EMPTY, "")};
    files_open(sdmcArchive);
}