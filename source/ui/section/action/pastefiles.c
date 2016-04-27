#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <3ds.h>

#include "action.h"
#include "clipboard.h"
#include "../task/task.h"
#include "../../error.h"
#include "../../info.h"
#include "../../list.h"
#include "../../prompt.h"
#include "../../ui.h"
#include "../../../core/linkedlist.h"
#include "../../../core/screen.h"
#include "../../../core/util.h"

typedef struct {
    linked_list* items;
    file_info* target;

    char** contents;
    list_item* currSrc;
    bool currExists;

    data_op_info pasteInfo;
    Handle cancelEvent;
} paste_files_data;

static void action_paste_files_get_dst_path(paste_files_data* data, u32 index, char* dstPath) {
    char baseDstPath[FILE_PATH_MAX];
    if(data->target->isDirectory) {
        strncpy(baseDstPath, data->target->path, FILE_PATH_MAX);
    } else {
        util_get_parent_path(baseDstPath, data->target->path, FILE_PATH_MAX);
    }

    util_get_parent_path(dstPath, clipboard_get_path(), FILE_PATH_MAX);
    snprintf(dstPath, FILE_PATH_MAX, "%s%s", baseDstPath, data->contents[index] + strlen(dstPath));
}

static Result action_paste_files_is_src_directory(void* data, u32 index, bool* isDirectory) {
    paste_files_data* pasteData = (paste_files_data*) data;

    *isDirectory = util_is_dir(clipboard_get_archive(), pasteData->contents[index]);
    return 0;
}

static Result action_paste_files_make_dst_directory(void* data, u32 index) {
    paste_files_data* pasteData = (paste_files_data*) data;

    list_item* old = pasteData->currSrc;
    task_create_file_item(&pasteData->currSrc, clipboard_get_archive(), pasteData->contents[index]);
    if(old != NULL) {
        task_free_file(old);
    }

    char dstPath[FILE_PATH_MAX];
    action_paste_files_get_dst_path(pasteData, index, dstPath);

    bool existed = util_exists(pasteData->target->archive, dstPath);

    Result res = 0;

    FS_Path* fsPath = util_make_path_utf8(dstPath);
    if(fsPath != NULL) {
        res = FSUSER_CreateDirectory(*pasteData->target->archive, *fsPath, 0);

        util_free_path_utf8(fsPath);
    } else {
        res = R_FBI_OUT_OF_MEMORY;
    }

    char parentPath[FILE_PATH_MAX];
    util_get_parent_path(parentPath, dstPath, FILE_PATH_MAX);

    if(!existed && strcmp(parentPath, pasteData->target->path) == 0) {
        list_item* dstItem = NULL;
        if(R_SUCCEEDED(res) && R_SUCCEEDED(task_create_file_item(&dstItem, pasteData->target->archive, dstPath))) {
            linked_list_add(pasteData->items, dstItem);
        }
    }

    return res;
}

static Result action_paste_files_open_src(void* data, u32 index, u32* handle) {
    paste_files_data* pasteData = (paste_files_data*) data;

    list_item* old = pasteData->currSrc;
    task_create_file_item(&pasteData->currSrc, clipboard_get_archive(), pasteData->contents[index]);
    if(old != NULL) {
        task_free_file(old);
    }

    Result res = 0;

    FS_Path* fsPath = util_make_path_utf8(pasteData->contents[index]);
    if(fsPath != NULL) {
        res = FSUSER_OpenFile(handle, *clipboard_get_archive(), *fsPath, FS_OPEN_READ, 0);

        util_free_path_utf8(fsPath);
    } else {
        res = R_FBI_OUT_OF_MEMORY;
    }

    return res;
}

static Result action_paste_files_close_src(void* data, u32 index, bool succeeded, u32 handle) {
    return FSFILE_Close(handle);
}

static Result action_paste_files_get_src_size(void* data, u32 handle, u64* size) {
    return FSFILE_GetSize(handle, size);
}

static Result action_paste_files_read_src(void* data, u32 handle, u32* bytesRead, void* buffer, u64 offset, u32 size) {
    return FSFILE_Read(handle, bytesRead, offset, buffer, size);
}

static Result action_paste_files_open_dst(void* data, u32 index, void* initialReadBlock, u32* handle) {
    paste_files_data* pasteData = (paste_files_data*) data;

    char dstPath[FILE_PATH_MAX];
    action_paste_files_get_dst_path(pasteData, index, dstPath);

    pasteData->currExists = util_exists(pasteData->target->archive, dstPath);

    Result res = 0;

    FS_Path* fsPath = util_make_path_utf8(dstPath);
    if(fsPath != NULL) {
        res = FSUSER_OpenFile(handle, *pasteData->target->archive, *fsPath, FS_OPEN_WRITE | FS_OPEN_CREATE, 0);

        util_free_path_utf8(fsPath);
    } else {
        res = R_FBI_OUT_OF_MEMORY;
    }

    return res;
}

static Result action_paste_files_close_dst(void* data, u32 index, bool succeeded, u32 handle) {
    paste_files_data* pasteData = (paste_files_data*) data;

    Result res = FSFILE_Close(handle);

    if(R_SUCCEEDED(res) && !pasteData->currExists) {
        char dstPath[FILE_PATH_MAX];
        action_paste_files_get_dst_path(pasteData, index, dstPath);

        char parentPath[FILE_PATH_MAX];
        util_get_parent_path(parentPath, dstPath, FILE_PATH_MAX);

        if(strcmp(parentPath, pasteData->target->path) == 0) {
            list_item* dstItem = NULL;
            if(R_SUCCEEDED(task_create_file_item(&dstItem, pasteData->target->archive, dstPath))) {
                linked_list_add(pasteData->items, dstItem);
            }
        }
    }

    return res;
}

static Result action_paste_files_write_dst(void* data, u32 handle, u32* bytesWritten, void* buffer, u64 offset, u32 size) {
    return FSFILE_Write(handle, bytesWritten, offset, buffer, size, 0);
}

static bool action_paste_files_error(void* data, u32 index, Result res) {
    paste_files_data* pasteData = (paste_files_data*) data;

    if(res == R_FBI_CANCELLED) {
        prompt_display("Failure", "Paste cancelled.", COLOR_TEXT, false, pasteData->target, NULL, ui_draw_file_info, NULL);
        return false;
    } else {
        char* path = pasteData->contents[index];

        volatile bool dismissed = false;
        error_display_res(&dismissed, pasteData->currSrc != NULL ? pasteData->currSrc->data : pasteData->target, ui_draw_file_info, res, "Failed to paste content.", path);

        while(!dismissed) {
            svcSleepThread(1000000);
        }
    }

    return index < pasteData->pasteInfo.total - 1;
}

static void action_paste_files_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    paste_files_data* pasteData = (paste_files_data*) data;

    if(pasteData->currSrc != NULL) {
        ui_draw_file_info(view, ((paste_files_data*) data)->currSrc->data, x1, y1, x2, y2);
    } else if(pasteData->target != NULL) {
        ui_draw_file_info(view, ((paste_files_data*) data)->target, x1, y1, x2, y2);
    }
}

static void action_paste_files_free_data(paste_files_data* data) {
    if(data->currSrc != NULL) {
        task_free_file(data->currSrc);
        data->currSrc = NULL;
    }

    util_free_contents(data->contents, data->pasteInfo.total);
    free(data);
}

static int action_paste_files_compare(const void* p1, const void* p2) {
    list_item* info1 = (list_item*) p1;
    list_item* info2 = (list_item*) p2;

    file_info* f1 = (file_info*) info1->data;
    file_info* f2 = (file_info*) info2->data;

    return strcasecmp(f1->name, f2->name);
}

static void action_paste_files_update(ui_view* view, void* data, float* progress, char* text) {
    paste_files_data* pasteData = (paste_files_data*) data;

    if(pasteData->pasteInfo.finished) {
        if(pasteData->target->archive->id == ARCHIVE_USER_SAVEDATA) {
            FSUSER_ControlArchive(*pasteData->target->archive, ARCHIVE_ACTION_COMMIT_SAVE_DATA, NULL, 0, NULL, 0);
        }

        linked_list_sort(pasteData->items, action_paste_files_compare);

        ui_pop();
        info_destroy(view);

        if(!pasteData->pasteInfo.premature) {
            prompt_display("Success", "Contents pasted.", COLOR_TEXT, false, pasteData->target, NULL, ui_draw_file_info, NULL);
        }

        action_paste_files_free_data(pasteData);

        return;
    }

    if(hidKeysDown() & KEY_B) {
        svcSignalEvent(pasteData->cancelEvent);
    }

    *progress = pasteData->pasteInfo.currTotal != 0 ? (float) ((double) pasteData->pasteInfo.currProcessed / (double) pasteData->pasteInfo.currTotal) : 0;
    snprintf(text, PROGRESS_TEXT_MAX, "%lu / %lu\n%.2f MB / %.2f MB", pasteData->pasteInfo.processed, pasteData->pasteInfo.total, pasteData->pasteInfo.currProcessed / 1024.0 / 1024.0, pasteData->pasteInfo.currTotal / 1024.0 / 1024.0);
}

static void action_paste_files_onresponse(ui_view* view, void* data, bool response) {
    paste_files_data* pasteData = (paste_files_data*) data;
    if(response) {
        pasteData->cancelEvent = task_data_op(&pasteData->pasteInfo);
        if(pasteData->cancelEvent != 0) {
            info_display("Pasting Contents", "Press B to cancel.", true, data, action_paste_files_update, action_paste_files_draw_top);
        } else {
            error_display(NULL, pasteData->target, ui_draw_file_info, "Failed to initiate paste operation.");
        }
    } else {
        action_paste_files_free_data(pasteData);
    }
}

void action_paste_contents(linked_list* items, list_item* selected, file_info* target) {
    if(!clipboard_has_contents()) {
        prompt_display("Failure", "Clipboard empty.", COLOR_TEXT, false, NULL, NULL, NULL, NULL);
        return;
    }

    paste_files_data* data = (paste_files_data*) calloc(1, sizeof(paste_files_data));
    if(data == NULL) {
        error_display(NULL, NULL, NULL, "Failed to allocate paste files data.");

        return;
    }

    data->items = items;
    data->target = target;

    data->currSrc = NULL;

    data->pasteInfo.data = data;

    data->pasteInfo.op = DATAOP_COPY;

    data->pasteInfo.copyEmpty = true;

    data->pasteInfo.isSrcDirectory = action_paste_files_is_src_directory;
    data->pasteInfo.makeDstDirectory = action_paste_files_make_dst_directory;

    data->pasteInfo.openSrc = action_paste_files_open_src;
    data->pasteInfo.closeSrc = action_paste_files_close_src;
    data->pasteInfo.getSrcSize = action_paste_files_get_src_size;
    data->pasteInfo.readSrc = action_paste_files_read_src;

    data->pasteInfo.openDst = action_paste_files_open_dst;
    data->pasteInfo.closeDst = action_paste_files_close_dst;
    data->pasteInfo.writeDst = action_paste_files_write_dst;

    data->pasteInfo.error = action_paste_files_error;

    data->cancelEvent = 0;

    Result res = 0;
    if(R_FAILED(res = util_populate_contents(&data->contents, &data->pasteInfo.total, clipboard_get_archive(), clipboard_get_path(), true, true, NULL, NULL))) {
        error_display_res(NULL, data->target, ui_draw_file_info, res, "Failed to retrieve content list.");

        free(data);
        return;
    }

    prompt_display("Confirmation", "Paste clipboard contents to the current directory?", COLOR_TEXT, true, data, NULL, action_paste_files_draw_top, action_paste_files_onresponse);
}