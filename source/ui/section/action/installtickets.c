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

typedef struct {
    linked_list* items;

    list_item* targetItem;
    file_info* target;

    linked_list contents;

    bool delete;
    bool cdn;
    bool selectCdnVersion;

    data_op_data installInfo;
} install_tickets_data;

static void action_install_tickets_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    install_tickets_data* installData = (install_tickets_data*) data;

    u32 curr = installData->installInfo.processed;
    if(curr < installData->installInfo.total) {
        ui_draw_file_info(view, ((list_item*) linked_list_get(&installData->contents, curr))->data, x1, y1, x2, y2);
    } else {
        ui_draw_file_info(view, installData->target, x1, y1, x2, y2);
    }
}

static Result action_install_tickets_is_src_directory(void* data, u32 index, bool* isDirectory) {
    *isDirectory = false;
    return 0;
}

static Result action_install_tickets_make_dst_directory(void* data, u32 index) {
    return 0;
}

static Result action_install_tickets_open_src(void* data, u32 index, u32* handle) {
    install_tickets_data* installData = (install_tickets_data*) data;

    file_info* info = (file_info*) ((list_item*) linked_list_get(&installData->contents, index))->data;

    Result res = 0;

    FS_Path* fsPath = util_make_path_utf8(info->path);
    if(fsPath != NULL) {
        res = FSUSER_OpenFile(handle, info->archive, *fsPath, FS_OPEN_READ, 0);

        util_free_path_utf8(fsPath);
    } else {
        res = R_FBI_OUT_OF_MEMORY;
    }

    return res;
}

static Result action_install_tickets_close_src(void* data, u32 index, bool succeeded, u32 handle) {
    install_tickets_data* installData = (install_tickets_data*) data;

    file_info* info = (file_info*) ((list_item*) linked_list_get(&installData->contents, index))->data;

    Result res = 0;

    if(R_SUCCEEDED(res = FSFILE_Close(handle)) && installData->delete && succeeded) {
        FS_Path* fsPath = util_make_path_utf8(info->path);
        if(fsPath != NULL) {
            if(R_SUCCEEDED(FSUSER_DeleteFile(info->archive, *fsPath))) {
                linked_list_iter iter;
                linked_list_iterate(installData->items, &iter);

                while(linked_list_iter_has_next(&iter)) {
                    list_item* item = (list_item*) linked_list_iter_next(&iter);
                    file_info* currInfo = (file_info*) item->data;

                    if(strncmp(currInfo->path, info->path, FILE_PATH_MAX) == 0) {
                        linked_list_iter_remove(&iter);
                        task_free_file(item);
                    }
                }
            }

            util_free_path_utf8(fsPath);
        } else {
            res = R_FBI_OUT_OF_MEMORY;
        }
    }

    return res;
}

static Result action_install_tickets_get_src_size(void* data, u32 handle, u64* size) {
    return FSFILE_GetSize(handle, size);
}

static Result action_install_tickets_read_src(void* data, u32 handle, u32* bytesRead, void* buffer, u64 offset, u32 size) {
    return FSFILE_Read(handle, bytesRead, offset, buffer, size);
}

static Result action_install_tickets_open_dst(void* data, u32 index, void* initialReadBlock, u64 size, u32* handle) {
    AM_DeleteTicket(((file_info*) ((list_item*) linked_list_get(&((install_tickets_data*) data)->contents, index))->data)->ticketInfo.titleId);
    return AM_InstallTicketBegin(handle);
}

static Result action_install_tickets_close_dst(void* data, u32 index, bool succeeded, u32 handle) {
    install_tickets_data* installData = (install_tickets_data*) data;

    if(succeeded) {
        Result res = AM_InstallTicketFinish(handle);
        if(R_SUCCEEDED(res) && installData->cdn) {
            volatile bool done = false;
            action_install_cdn_noprompt(&done, &((file_info*) ((list_item*) linked_list_get(&installData->contents, index))->data)->ticketInfo, false, installData->selectCdnVersion);

            while(!done) {
                svcSleepThread(100000000);
            }
        }

        return res;
    } else {
        return AM_InstallTicketAbort(handle);
    }
}

static Result action_install_tickets_write_dst(void* data, u32 handle, u32* bytesWritten, void* buffer, u64 offset, u32 size) {
    return FSFILE_Write(handle, bytesWritten, offset, buffer, size, 0);
}

static Result action_install_tickets_suspend_copy(void* data, u32 index, u32* srcHandle, u32* dstHandle) {
    return 0;
}

static Result action_install_tickets_restore_copy(void* data, u32 index, u32* srcHandle, u32* dstHandle) {
    return 0;
}

static Result action_install_tickets_suspend(void* data, u32 index) {
    return 0;
}

static Result action_install_tickets_restore(void* data, u32 index) {
    return 0;
}

static bool action_install_tickets_error(void* data, u32 index, Result res) {
    install_tickets_data* installData = (install_tickets_data*) data;

    if(res == R_FBI_CANCELLED) {
        prompt_display_notify("Failure", "Install cancelled.", COLOR_TEXT, NULL, NULL, NULL);
        return false;
    } else {
        ui_view* view = error_display_res(data, action_install_tickets_draw_top, res, "Failed to install ticket.");
        if(view != NULL) {
            svcWaitSynchronization(view->active, U64_MAX);
        }
    }

    return index < installData->installInfo.total - 1;
}

static void action_install_tickets_free_data(install_tickets_data* data) {
    task_clear_files(&data->contents);
    linked_list_destroy(&data->contents);

    if(data->targetItem != NULL) {
        task_free_file(data->targetItem);
        data->targetItem = NULL;
        data->target = NULL;
    }

    free(data);
}

static void action_install_tickets_update(ui_view* view, void* data, float* progress, char* text) {
    install_tickets_data* installData = (install_tickets_data*) data;

    if(installData->installInfo.finished) {
        if(installData->delete) {
            FSUSER_ControlArchive(installData->target->archive, ARCHIVE_ACTION_COMMIT_SAVE_DATA, NULL, 0, NULL, 0);
        }

        ui_pop();
        info_destroy(view);

        if(R_SUCCEEDED(installData->installInfo.result)) {
            prompt_display_notify("Success", "Install finished.", COLOR_TEXT, NULL, NULL, NULL);
        }

        action_install_tickets_free_data(installData);

        return;
    }

    if((hidKeysDown() & KEY_B) && !installData->installInfo.finished) {
        svcSignalEvent(installData->installInfo.cancelEvent);
    }

    *progress = installData->installInfo.currTotal != 0 ? (float) ((double) installData->installInfo.currProcessed / (double) installData->installInfo.currTotal) : 0;
    snprintf(text, PROGRESS_TEXT_MAX, "%lu / %lu\n%.2f %s / %.2f %s\n%.2f %s/s", installData->installInfo.processed, installData->installInfo.total, util_get_display_size(installData->installInfo.currProcessed), util_get_display_size_units(installData->installInfo.currProcessed), util_get_display_size(installData->installInfo.currTotal), util_get_display_size_units(installData->installInfo.currTotal), util_get_display_size(installData->installInfo.copyBytesPerSecond), util_get_display_size_units(installData->installInfo.copyBytesPerSecond));
}

#define CDN_PROMPT_DEFAULT_VERSION 0
#define CDN_PROMPT_SELECT_VERSION 1
#define CDN_PROMPT_NO 2

static void action_install_tickets_cdn_check_onresponse(ui_view* view, void* data, u32 response) {
    install_tickets_data* installData = (install_tickets_data*) data;

    installData->cdn = response != CDN_PROMPT_NO;
    installData->selectCdnVersion = response == CDN_PROMPT_SELECT_VERSION;

    Result res = task_data_op(&installData->installInfo);
    if(R_SUCCEEDED(res)) {
        info_display("Installing ticket(s)", "Press B to cancel.", true, data, action_install_tickets_update, action_install_tickets_draw_top);
    } else {
        error_display_res(NULL, NULL, res, "Failed to initiate ticket installation.");

        action_install_tickets_free_data(installData);
    }
}

static void action_install_tickets_onresponse(ui_view* view, void* data, u32 response) {
    if(response == PROMPT_YES) {
        static const char* options[3] = {"Default\nVersion", "Select\nVersion", "No"};
        static u32 optionButtons[3] = {KEY_A, KEY_X, KEY_B};
        prompt_display_multi_choice("Optional", "Install ticket titles from CDN?", COLOR_TEXT, options, optionButtons, 3, data, action_install_tickets_draw_top, action_install_tickets_cdn_check_onresponse);
    } else {
        action_install_tickets_free_data((install_tickets_data*) data);
    }
}

typedef struct {
    install_tickets_data* installData;

    const char* message;

    populate_files_data popData;
} install_tickets_loading_data;

static void action_install_tickets_loading_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    action_install_tickets_draw_top(view, ((install_tickets_loading_data*) data)->installData, x1, y1, x2, y2);
}

static void action_install_tickets_loading_update(ui_view* view, void* data, float* progress, char* text)  {
    install_tickets_loading_data* loadingData = (install_tickets_loading_data*) data;

    if(loadingData->popData.finished) {
        ui_pop();
        info_destroy(view);

        if(R_SUCCEEDED(loadingData->popData.result)) {
            loadingData->installData->installInfo.total = linked_list_size(&loadingData->installData->contents);
            loadingData->installData->installInfo.processed = loadingData->installData->installInfo.total;

            prompt_display_yes_no("Confirmation", loadingData->message, COLOR_TEXT, loadingData->installData, action_install_tickets_draw_top, action_install_tickets_onresponse);
        } else {
            error_display_res(NULL, NULL, loadingData->popData.result, "Failed to populate ticket list.");

            action_install_tickets_free_data(loadingData->installData);
        }

        free(loadingData);
        return;
    }

    if((hidKeysDown() & KEY_B) && !loadingData->popData.finished) {
        svcSignalEvent(loadingData->popData.cancelEvent);
    }

    snprintf(text, PROGRESS_TEXT_MAX, "Fetching ticket list...");
}

static void action_install_tickets_internal(linked_list* items, list_item* selected, const char* message, bool delete) {
    install_tickets_data* data = (install_tickets_data*) calloc(1, sizeof(install_tickets_data));
    if(data == NULL) {
        error_display(NULL, NULL, "Failed to allocate install tickets data.");

        return;
    }

    data->items = items;

    file_info* targetInfo = (file_info*) selected->data;
    Result targetCreateRes = task_create_file_item(&data->targetItem, targetInfo->archive, targetInfo->path, targetInfo->attributes);
    if(R_FAILED(targetCreateRes)) {
        error_display_res(NULL, NULL, targetCreateRes, "Failed to create target file item.");

        action_install_tickets_free_data(data);
        return;
    }

    data->target = (file_info*) data->targetItem->data;

    data->delete = delete;

    data->installInfo.data = data;

    data->installInfo.op = DATAOP_COPY;

    data->installInfo.copyBufferSize = 256 * 1024;
    data->installInfo.copyEmpty = false;

    data->installInfo.isSrcDirectory = action_install_tickets_is_src_directory;
    data->installInfo.makeDstDirectory = action_install_tickets_make_dst_directory;

    data->installInfo.openSrc = action_install_tickets_open_src;
    data->installInfo.closeSrc = action_install_tickets_close_src;
    data->installInfo.getSrcSize = action_install_tickets_get_src_size;
    data->installInfo.readSrc = action_install_tickets_read_src;

    data->installInfo.openDst = action_install_tickets_open_dst;
    data->installInfo.closeDst = action_install_tickets_close_dst;
    data->installInfo.writeDst = action_install_tickets_write_dst;

    data->installInfo.suspendCopy = action_install_tickets_suspend_copy;
    data->installInfo.restoreCopy = action_install_tickets_restore_copy;

    data->installInfo.suspend = action_install_tickets_suspend;
    data->installInfo.restore = action_install_tickets_restore;

    data->installInfo.error = action_install_tickets_error;

    data->installInfo.finished = true;

    linked_list_init(&data->contents);

    install_tickets_loading_data* loadingData = (install_tickets_loading_data*) calloc(1, sizeof(install_tickets_loading_data));
    if(loadingData == NULL) {
        error_display(NULL, NULL, "Failed to allocate loading data.");

        action_install_tickets_free_data(data);
        return;
    }

    loadingData->installData = data;
    loadingData->message = message;

    loadingData->popData.items = &data->contents;
    loadingData->popData.archive = data->target->archive;
    strncpy(loadingData->popData.path, data->target->path, FILE_PATH_MAX);
    loadingData->popData.recursive = false;
    loadingData->popData.includeBase = !(data->target->attributes & FS_ATTRIBUTE_DIRECTORY);
    loadingData->popData.filter = util_filter_tickets;
    loadingData->popData.filterData = NULL;

    Result listRes = task_populate_files(&loadingData->popData);
    if(R_FAILED(listRes)) {
        error_display_res(NULL, NULL, listRes, "Failed to initiate ticket list population.");

        free(loadingData);
        action_install_tickets_free_data(data);
        return;
    }

    info_display("Loading", "Press B to cancel.", false, loadingData, action_install_tickets_loading_update, action_install_tickets_loading_draw_top);
}

void action_install_ticket(linked_list* items, list_item* selected) {
    action_install_tickets_internal(items, selected, "Install the selected ticket?", false);
}

void action_install_ticket_delete(linked_list* items, list_item* selected) {
    action_install_tickets_internal(items, selected, "Install and delete the selected ticket?", true);
}

void action_install_tickets(linked_list* items, list_item* selected) {
    action_install_tickets_internal(items, selected, "Install all tickets in the current directory?", false);
}

void action_install_tickets_delete(linked_list* items, list_item* selected) {
    action_install_tickets_internal(items, selected, "Install and delete all tickets in the current directory?", true);
}