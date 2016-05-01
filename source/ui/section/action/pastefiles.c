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

    linked_list contents;
    bool currExists;

    data_op_data pasteInfo;
} paste_files_data;

static void action_paste_files_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    paste_files_data* pasteData = (paste_files_data*) data;

    u32 curr = pasteData->pasteInfo.processed;
    if(curr < pasteData->pasteInfo.total) {
        ui_draw_file_info(view, ((list_item*) linked_list_get(&pasteData->contents, curr))->data, x1, y1, x2, y2);
    } else if(pasteData->target != NULL) {
        ui_draw_file_info(view, pasteData->target, x1, y1, x2, y2);
    }
}

static void action_paste_files_get_dst_path(paste_files_data* data, u32 index, char* dstPath) {
    char baseSrcPath[FILE_PATH_MAX];
    if(clipboard_is_contents_only()) {
        strncpy(baseSrcPath, clipboard_get_path(), FILE_PATH_MAX);
    } else {
        util_get_parent_path(baseSrcPath, clipboard_get_path(), FILE_PATH_MAX);
    }

    char baseDstPath[FILE_PATH_MAX];
    if(data->target->isDirectory) {
        strncpy(baseDstPath, data->target->path, FILE_PATH_MAX);
    } else {
        util_get_parent_path(baseDstPath, data->target->path, FILE_PATH_MAX);
    }

    snprintf(dstPath, FILE_PATH_MAX, "%s%s", baseDstPath, ((file_info*) ((list_item*) linked_list_get(&data->contents, index))->data)->path + strlen(baseSrcPath));
}

static Result action_paste_files_is_src_directory(void* data, u32 index, bool* isDirectory) {
    paste_files_data* pasteData = (paste_files_data*) data;

    *isDirectory = ((file_info*) ((list_item*) linked_list_get(&pasteData->contents, index))->data)->isDirectory;
    return 0;
}

static Result action_paste_files_make_dst_directory(void* data, u32 index) {
    paste_files_data* pasteData = (paste_files_data*) data;

    Result res = 0;

    char dstPath[FILE_PATH_MAX];
    action_paste_files_get_dst_path(pasteData, index, dstPath);

    if(R_SUCCEEDED(res = util_ensure_dir(pasteData->target->archive, dstPath))) {
        char parentPath[FILE_PATH_MAX];
        util_get_parent_path(parentPath, dstPath, FILE_PATH_MAX);

        if(strncmp(parentPath, pasteData->target->path, FILE_PATH_MAX) == 0) {
            list_item* dstItem = NULL;
            if(R_SUCCEEDED(res) && R_SUCCEEDED(task_create_file_item(&dstItem, pasteData->target->archive, dstPath))) {
                linked_list_add(pasteData->items, dstItem);
            }
        }
    }

    return res;
}

static Result action_paste_files_open_src(void* data, u32 index, u32* handle) {
    paste_files_data* pasteData = (paste_files_data*) data;

    Result res = 0;

    FS_Path* fsPath = util_make_path_utf8(((file_info*) ((list_item*) linked_list_get(&pasteData->contents, index))->data)->path);
    if(fsPath != NULL) {
        res = FSUSER_OpenFile(handle, clipboard_get_archive(), *fsPath, FS_OPEN_READ, 0);

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

    Result res = 0;

    char dstPath[FILE_PATH_MAX];
    action_paste_files_get_dst_path(pasteData, index, dstPath);

    FS_Path* fsPath = util_make_path_utf8(dstPath);
    if(fsPath != NULL) {
        Handle currHandle;
        pasteData->currExists = R_SUCCEEDED(FSUSER_OpenFile(&currHandle, pasteData->target->archive, *fsPath, FS_OPEN_READ, 0));
        if(pasteData->currExists) {
            FSFILE_Close(currHandle);
        }

        res = FSUSER_OpenFile(handle, pasteData->target->archive, *fsPath, FS_OPEN_WRITE | FS_OPEN_CREATE, 0);

        util_free_path_utf8(fsPath);
    } else {
        res = R_FBI_OUT_OF_MEMORY;
    }

    return res;
}

static Result action_paste_files_close_dst(void* data, u32 index, bool succeeded, u32 handle) {
    paste_files_data* pasteData = (paste_files_data*) data;

    Result res = 0;

    if(R_SUCCEEDED(res = FSFILE_Close(handle)) && !pasteData->currExists) {
        char dstPath[FILE_PATH_MAX];
        action_paste_files_get_dst_path(pasteData, index, dstPath);

        char parentPath[FILE_PATH_MAX];
        util_get_parent_path(parentPath, dstPath, FILE_PATH_MAX);

        if(strncmp(parentPath, pasteData->target->path, FILE_PATH_MAX) == 0) {
            list_item* dstItem = NULL;
            if(R_SUCCEEDED(task_create_file_item(&dstItem, pasteData->target->archive, dstPath))) {
                linked_list_add(pasteData->items, dstItem);

                file_info* dstInfo = (file_info*) dstItem->data;

                if(dstInfo->isCia) {
                    pasteData->target->containsCias = true;
                } else if(dstInfo->isTicket) {
                    pasteData->target->containsTickets = true;
                }
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
        prompt_display("Failure", "Paste cancelled.", COLOR_TEXT, false, NULL, NULL, NULL, NULL);
        return false;
    } else {
        volatile bool dismissed = false;
        error_display_res(&dismissed, data, action_paste_files_draw_top, res, "Failed to paste content.");

        while(!dismissed) {
            svcSleepThread(1000000);
        }
    }

    return index < pasteData->pasteInfo.total - 1;
}

static void action_paste_files_free_data(paste_files_data* data) {
    task_clear_files(&data->contents);
    linked_list_destroy(&data->contents);
    free(data);
}

static int action_paste_files_compare(const void** p1, const void** p2) {
    list_item* info1 = *(list_item**) p1;
    list_item* info2 = *(list_item**) p2;

    file_info* f1 = (file_info*) info1->data;
    file_info* f2 = (file_info*) info2->data;

    if(f1->isDirectory && !f2->isDirectory) {
        return -1;
    } else if(!f1->isDirectory && f2->isDirectory) {
        return 1;
    } else {
        return strcasecmp(f1->name, f2->name);
    }
}

static void action_paste_files_update(ui_view* view, void* data, float* progress, char* text) {
    paste_files_data* pasteData = (paste_files_data*) data;

    if(pasteData->pasteInfo.finished) {
        FSUSER_ControlArchive(pasteData->target->archive, ARCHIVE_ACTION_COMMIT_SAVE_DATA, NULL, 0, NULL, 0);

        linked_list_sort(pasteData->items, action_paste_files_compare);

        ui_pop();
        info_destroy(view);

        if(R_SUCCEEDED(pasteData->pasteInfo.result)) {
            prompt_display("Success", "Contents pasted.", COLOR_TEXT, false, NULL, NULL, NULL, NULL);
        }

        action_paste_files_free_data(pasteData);

        return;
    }

    if((hidKeysDown() & KEY_B) && !pasteData->pasteInfo.finished) {
        svcSignalEvent(pasteData->pasteInfo.cancelEvent);
    }

    *progress = pasteData->pasteInfo.currTotal != 0 ? (float) ((double) pasteData->pasteInfo.currProcessed / (double) pasteData->pasteInfo.currTotal) : 0;
    snprintf(text, PROGRESS_TEXT_MAX, "%lu / %lu\n%.2f MiB / %.2f MiB", pasteData->pasteInfo.processed, pasteData->pasteInfo.total, pasteData->pasteInfo.currProcessed / 1024.0 / 1024.0, pasteData->pasteInfo.currTotal / 1024.0 / 1024.0);
}

static void action_paste_files_onresponse(ui_view* view, void* data, bool response) {
    paste_files_data* pasteData = (paste_files_data*) data;
    if(response) {
        Result res = task_data_op(&pasteData->pasteInfo);
        if(R_SUCCEEDED(res)) {
            info_display("Pasting Contents", "Press B to cancel.", true, data, action_paste_files_update, action_paste_files_draw_top);
        } else {
            error_display_res(NULL, pasteData->target, ui_draw_file_info, res, "Failed to initiate paste operation.");

            action_paste_files_free_data(pasteData);
        }
    } else {
        action_paste_files_free_data(pasteData);
    }
}

void action_paste_contents(linked_list* items, list_item* selected) {
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
    data->target = (file_info*) selected->data;

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

    data->pasteInfo.finished = true;

    list_item* clipboardItem = NULL;
    Result createRes = 0;
    if(R_FAILED(createRes = task_create_file_item(&clipboardItem, clipboard_get_archive(), clipboard_get_path()))) {
        error_display_res(NULL, NULL, NULL, createRes, "Failed to retrieve clipboard content info.");

        action_paste_files_free_data(data);
        return;
    }

    linked_list_init(&data->contents);

    populate_files_data popData;
    popData.items = &data->contents;
    popData.base = (file_info*) clipboardItem->data;
    popData.recursive = true;
    popData.includeBase = !clipboard_is_contents_only() || !util_is_dir(clipboard_get_archive(), clipboard_get_path());
    popData.dirsFirst = true;

    Result listRes = task_populate_files(&popData);
    if(R_FAILED(listRes)) {
        error_display_res(NULL, NULL, NULL, listRes, "Failed to initiate clipboard content list population.");

        task_free_file(clipboardItem);
        action_paste_files_free_data(data);
        return;
    }

    while(!popData.finished) {
        svcSleepThread(1000000);
    }

    task_free_file(clipboardItem);

    if(R_FAILED(popData.result)) {
        error_display_res(NULL, NULL, NULL, popData.result, "Failed to populate clipboard content list.");

        action_paste_files_free_data(data);
        return;
    }

    data->pasteInfo.total = linked_list_size(&data->contents);
    data->pasteInfo.processed = data->pasteInfo.total;

    prompt_display("Confirmation", "Paste clipboard contents to the current directory?", COLOR_TEXT, true, data, NULL, action_paste_files_draw_top, action_paste_files_onresponse);
}