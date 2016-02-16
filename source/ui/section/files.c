#include <dirent.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <3ds.h>
#include <3ds/services/fs.h>

#include "action/action.h"
#include "section.h"
#include "../error.h"
#include "../../util.h"
#include "task.h"

typedef struct {
    bool setup;
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

static void files_action_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    ui_draw_file_info(view, data, x1, y1, x2, y2);
}

static void files_action_update(ui_view* view, void* data, list_item** items, u32** itemCount, list_item* selected, bool selectedTouched) {
    file_info* fileInfo = (file_info*) data;

    if(hidKeysDown() & KEY_B) {
        list_destroy(view);
        ui_pop();
        return;
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        void(*action)(file_info*) = (void(*)(file_info*)) selected->data;

        list_destroy(view);
        ui_pop();

        action(fileInfo);
        return;
    }

    if(fileInfo->isDirectory) {
        if(fileInfo->containsCias) {
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
        if(fileInfo->isCia) {
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

static ui_view* files_action_create(file_info* info) {
    return list_create(info->isDirectory ? "Directory Action" : "File Action", "A: Select, B: Return", info, files_action_update, files_action_draw_top);
}

static void files_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, list_item* selected) {
    if(selected != NULL && selected->data != NULL) {
        ui_draw_file_info(view, selected->data, x1, y1, x2, y2);
    }
}

static void files_update(ui_view* view, void* data, list_item** items, u32** itemCount, list_item* selected, bool selectedTouched) {
    files_data* filesData = (files_data*) data;

    if(hidKeysDown() & KEY_B) {
        if(strcmp(filesData->path, "/") == 0) {
            if(filesData->archive.handle != 0) {
                FSUSER_CloseArchive(&filesData->archive);
                filesData->archive.handle = 0;
            }

            if(filesData->archivePath != NULL) {
                free(filesData->archivePath);
                filesData->archivePath = NULL;
            }

            free(data);
            list_destroy(view);

            ui_pop();
            return;
        } else if(*items != NULL && *itemCount != NULL) {
            for(u32 i = 0; i < **itemCount; i++) {
                char* name = (*items)[i].name;
                file_info* fileInfo = (*items)[i].data;
                if(fileInfo != NULL && strcmp(name, "..") == 0) {
                    strncpy(filesData->path, fileInfo->path, PATH_MAX);
                    task_refresh_files();
                    break;
                }
            }
        }
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        file_info* fileInfo = (file_info*) selected->data;

        if(strcmp(selected->name, ".") == 0) {
            ui_push(files_action_create(fileInfo));
        } else if(strcmp(selected->name, "..") == 0) {
            strncpy(filesData->path, fileInfo->path, PATH_MAX);
            task_refresh_files();
        } else {
            if(util_is_dir(&filesData->archive, fileInfo->path)) {
                strncpy(filesData->path, fileInfo->path, PATH_MAX);
                task_refresh_files();
            } else {
                ui_push(files_action_create(fileInfo));
            }
        }
    }

    if(hidKeysDown() & KEY_X) {
        task_refresh_files();
    }

    if(!filesData->setup || task_get_files_archive() != &filesData->archive || task_get_files_path() != filesData->path) {
        filesData->setup = true;

        task_set_files_archive(&filesData->archive);
        task_set_files_path(filesData->path);

        task_refresh_files();
    }

    if(*itemCount != task_get_files_count() || *items != task_get_files()) {
        *itemCount = task_get_files_count();
        *items = task_get_files();
    }
}

void files_open(FS_Archive archive) {
    files_data* data = (files_data*) calloc(1, sizeof(files_data));
    data->setup = false;
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

void files_open_ctrnand() {
    FS_Archive ctrNandArchive = {ARCHIVE_NAND_CTR_FS, {PATH_BINARY, 0, (void*) ""}};
    files_open(ctrNandArchive);
}

void files_open_twlnand() {
    FS_Archive twlNandArchive = {ARCHIVE_NAND_TWL_FS, {PATH_BINARY, 0, (void*) ""}};
    files_open(twlNandArchive);
}