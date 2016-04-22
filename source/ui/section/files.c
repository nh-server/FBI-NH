#include <dirent.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <3ds.h>

#include "action/action.h"
#include "section.h"
#include "../error.h"
#include "../../screen.h"
#include "../../util.h"

#define FILES_MAX 1024

typedef struct {
    list_item items[FILES_MAX];
    u32 count;
    Handle cancelEvent;
    bool populated;

    FS_Archive archive;
    void* archivePath;

    file_info currDir;
    file_info parentDir;
} files_data;

#define FILES_ACTION_COUNT 3

static u32 files_action_count = FILES_ACTION_COUNT;
static list_item files_action_items[FILES_ACTION_COUNT] = {
        {"Delete", COLOR_TEXT, action_delete_contents},
        {"Copy", COLOR_TEXT, action_copy_contents},
        {"Paste", COLOR_TEXT, action_paste_contents},
};

#define CIA_FILES_ACTION_COUNT 5

static u32 cia_files_action_count = CIA_FILES_ACTION_COUNT;
static list_item cia_files_action_items[CIA_FILES_ACTION_COUNT] = {
        {"Install CIA", COLOR_TEXT, action_install_cias},
        {"Install and delete CIA", COLOR_TEXT, action_install_cias_delete},
        {"Delete", COLOR_TEXT, action_delete_contents},
        {"Copy", COLOR_TEXT, action_copy_contents},
        {"Paste", COLOR_TEXT, action_paste_contents},
};

#define TICKET_FILES_ACTION_COUNT 4

static u32 ticket_files_action_count = TICKET_FILES_ACTION_COUNT;
static list_item ticket_files_action_items[TICKET_FILES_ACTION_COUNT] = {
        {"Install ticket", COLOR_TEXT, action_install_tickets},
        {"Delete", COLOR_TEXT, action_delete_contents},
        {"Copy", COLOR_TEXT, action_copy_contents},
        {"Paste", COLOR_TEXT, action_paste_contents},
};

#define DIRECTORIES_ACTION_COUNT 4

static u32 directories_action_count = DIRECTORIES_ACTION_COUNT;
static list_item directories_action_items[DIRECTORIES_ACTION_COUNT] = {
        {"Delete all contents", COLOR_TEXT, action_delete_dir_contents},
        {"Delete", COLOR_TEXT, action_delete_dir},
        {"Copy", COLOR_TEXT, action_copy_contents},
        {"Paste", COLOR_TEXT, action_paste_contents},
};

#define CIA_DIRECTORIES_ACTION_COUNT 7

static u32 cia_directories_action_count = CIA_DIRECTORIES_ACTION_COUNT;
static list_item cia_directories_action_items[CIA_DIRECTORIES_ACTION_COUNT] = {
        {"Install all CIAs", COLOR_TEXT, action_install_cias},
        {"Install and delete all CIAs", COLOR_TEXT, action_install_cias_delete},
        {"Delete all CIAs", COLOR_TEXT, action_delete_dir_cias},
        {"Delete all contents", COLOR_TEXT, action_delete_dir_contents},
        {"Delete", COLOR_TEXT, action_delete_dir},
        {"Copy", COLOR_TEXT, action_copy_contents},
        {"Paste", COLOR_TEXT, action_paste_contents},
};

#define TICKET_DIRECTORIES_ACTION_COUNT 5

static u32 ticket_directories_action_count = TICKET_DIRECTORIES_ACTION_COUNT;
static list_item ticket_directories_action_items[TICKET_DIRECTORIES_ACTION_COUNT] = {
        {"Install all tickets", COLOR_TEXT, action_install_tickets},
        {"Delete all contents", COLOR_TEXT, action_delete_dir_contents},
        {"Delete", COLOR_TEXT, action_delete_dir},
        {"Copy", COLOR_TEXT, action_copy_contents},
        {"Paste", COLOR_TEXT, action_paste_contents},
};

#define CIA_TICKET_DIRECTORIES_ACTION_COUNT 8

static u32 cia_ticket_directories_action_count = CIA_TICKET_DIRECTORIES_ACTION_COUNT;
static list_item cia_ticket_directories_action_items[CIA_TICKET_DIRECTORIES_ACTION_COUNT] = {
        {"Install all CIAs", COLOR_TEXT, action_install_cias},
        {"Install and delete all CIAs", COLOR_TEXT, action_install_cias_delete},
        {"Install all tickets", COLOR_TEXT, action_install_tickets},
        {"Delete all CIAs", COLOR_TEXT, action_delete_dir_cias},
        {"Delete all contents", COLOR_TEXT, action_delete_dir_contents},
        {"Delete", COLOR_TEXT, action_delete_dir},
        {"Copy", COLOR_TEXT, action_copy_contents},
        {"Paste", COLOR_TEXT, action_paste_contents},
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
        ui_pop();
        list_destroy(view);

        free(data);

        return;
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        void(*action)(file_info*, bool*) = (void(*)(file_info*, bool*)) selected->data;

        ui_pop();
        list_destroy(view);

        action(actionData->info, actionData->populated);

        free(data);

        return;
    }

    if(actionData->info->isDirectory) {
        if(actionData->info->containsCias && actionData->info->containsTickets) {
            if(*itemCount != &cia_ticket_directories_action_count || *items != cia_ticket_directories_action_items) {
                *itemCount = &cia_ticket_directories_action_count;
                *items = cia_ticket_directories_action_items;
            }
        } else if(actionData->info->containsCias) {
            if(*itemCount != &cia_directories_action_count || *items != cia_directories_action_items) {
                *itemCount = &cia_directories_action_count;
                *items = cia_directories_action_items;
            }
        } else if(actionData->info->containsTickets) {
            if(*itemCount != &ticket_directories_action_count || *items != ticket_directories_action_items) {
                *itemCount = &ticket_directories_action_count;
                *items = ticket_directories_action_items;
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
        } else if(actionData->info->isTicket) {
            if(*itemCount != &ticket_files_action_count || *items != ticket_files_action_items) {
                *itemCount = &ticket_files_action_count;
                *items = ticket_files_action_items;
            }
        } else {
            if(*itemCount != &files_action_count || *items != files_action_items) {
                *itemCount = &files_action_count;
                *items = files_action_items;
            }
        }
    }
}

static void files_action_open(file_info* info, bool* populated) {
    files_action_data* data = (files_action_data*) calloc(1, sizeof(files_action_data));
    if(data == NULL) {
        error_display(NULL, NULL, NULL, "Failed to allocate files action data.");

        return;
    }

    data->info = info;
    data->populated = populated;

    list_display(info->isDirectory ? "Directory Action" : "File Action", "A: Select, B: Return", data, files_action_update, files_action_draw_top);
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

    while(!util_is_dir(&listData->archive, listData->currDir.path)) {
        char parentPath[PATH_MAX];

        util_get_parent_path(parentPath, listData->currDir.path, PATH_MAX);
        strncpy(listData->currDir.path, parentPath, PATH_MAX);
        util_get_path_file(listData->currDir.name, listData->currDir.path, NAME_MAX);

        util_get_parent_path(parentPath, listData->currDir.path, PATH_MAX);
        strncpy(listData->parentDir.path, parentPath, PATH_MAX);
        util_get_path_file(listData->parentDir.name, listData->parentDir.path, NAME_MAX);
    }

    listData->cancelEvent = task_populate_files(listData->items, &listData->count, FILES_MAX, &listData->currDir);
    listData->populated = true;
}

static void files_navigate(files_data* listData, const char* path) {
    strncpy(listData->currDir.path, path, PATH_MAX);
    util_get_path_file(listData->currDir.name, listData->currDir.path, NAME_MAX);

    char parentPath[PATH_MAX];
    util_get_parent_path(parentPath, listData->currDir.path, PATH_MAX);
    strncpy(listData->parentDir.path, parentPath, PATH_MAX);
    util_get_path_file(listData->parentDir.name, listData->parentDir.path, NAME_MAX);

    files_repopulate(listData);
}

static void files_update(ui_view* view, void* data, list_item** items, u32** itemCount, list_item* selected, bool selectedTouched) {
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
            list_destroy(view);

            task_clear_files(listData->items, &listData->count);
            free(listData);
            return;
        } else {
            files_navigate(listData, listData->parentDir.path);
        }
    }

    if(hidKeysDown() & KEY_Y) {
        files_action_open(&listData->currDir, &listData->populated);
        return;
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        file_info* fileInfo = (file_info*) selected->data;

        if(util_is_dir(&listData->archive, fileInfo->path)) {
            files_navigate(listData, fileInfo->path);
        } else {
            files_action_open(fileInfo, &listData->populated);
            return;
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
    snprintf(data->currDir.path, PATH_MAX, "/");
    util_get_path_file(data->currDir.name, data->currDir.path, NAME_MAX);
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