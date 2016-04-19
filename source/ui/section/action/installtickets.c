#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <3ds.h>

#include "action.h"
#include "../task/task.h"
#include "../../error.h"
#include "../../progressbar.h"
#include "../../prompt.h"
#include "../../../screen.h"
#include "../../../util.h"

typedef struct {
    file_info* base;
    char** contents;

    copy_data_info installInfo;
    Handle cancelEvent;
} install_tickets_data;

Result action_install_tickets_is_src_directory(void* data, u32 index, bool* isDirectory) {
    *isDirectory = false;
    return 0;
}

Result action_install_tickets_make_dst_directory(void* data, u32 index) {
    return 0;
}

Result action_install_tickets_open_src(void* data, u32 index, u32* handle) {
    install_tickets_data* installData = (install_tickets_data*) data;

    return FSUSER_OpenFile(handle, *installData->base->archive, fsMakePath(PATH_ASCII, installData->contents[index]), FS_OPEN_READ, 0);
}

Result action_install_tickets_close_src(void* data, u32 index, bool succeeded, u32 handle) {
    return FSFILE_Close(handle);
}

Result action_install_tickets_get_src_size(void* data, u32 handle, u64* size) {
    return FSFILE_GetSize(handle, size);
}

Result action_install_tickets_read_src(void* data, u32 handle, u32* bytesRead, void* buffer, u64 offset, u32 size) {
    return FSFILE_Read(handle, bytesRead, offset, buffer, size);
}

Result action_install_tickets_open_dst(void* data, u32 index, void* initialReadBlock, u32* handle) {
    return AM_InstallTicketBegin(handle);
}

Result action_install_tickets_close_dst(void* data, u32 index, bool succeeded, u32 handle) {
    if(succeeded) {
        return AM_InstallTicketFinalize(handle);
    } else {
        return AM_InstallTicketAbort(handle);
    }
}

Result action_install_tickets_write_dst(void* data, u32 handle, u32* bytesWritten, void* buffer, u64 offset, u32 size) {
    return FSFILE_Write(handle, bytesWritten, offset, buffer, size, 0);
}

bool action_install_tickets_result_error(void* data, u32 index, Result res) {
    install_tickets_data* installData = (install_tickets_data*) data;

    if(res == MAKERESULT(RL_PERMANENT, RS_CANCELED, RM_APPLICATION, RD_CANCEL_REQUESTED)) {
        ui_push(prompt_create("Failure", "Install cancelled.", COLOR_TEXT, false, installData->base, NULL, ui_draw_file_info, NULL));
        return false;
    } else {
        char* path = installData->contents[index];

        volatile bool dismissed = false;
        if(strlen(path) > 48) {
            error_display_res(&dismissed, installData->base, ui_draw_file_info, res, "Failed to install ticket.\n%.45s...", path);
        } else {
            error_display_res(&dismissed, installData->base, ui_draw_file_info, res, "Failed to install ticket.\n%.48s", path);
        }

        while(!dismissed) {
            svcSleepThread(1000000);
        }
    }

    return index < installData->installInfo.total - 1;
}

bool action_install_tickets_io_error(void* data, u32 index, int err) {
    install_tickets_data* installData = (install_tickets_data*) data;

    char* path = installData->contents[index];

    volatile bool dismissed = false;
    if(strlen(path) > 48) {
        error_display_errno(&dismissed, installData->base, ui_draw_file_info, err, "Failed to install ticket.\n%.45s...", path);
    } else {
        error_display_errno(&dismissed, installData->base, ui_draw_file_info, err, "Failed to install ticket.\n%.48s", path);
    }

    while(!dismissed) {
        svcSleepThread(1000000);
    }

    return index < installData->installInfo.total - 1;
}

static void action_install_tickets_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    ui_draw_file_info(view, ((install_tickets_data*) data)->base, x1, y1, x2, y2);
}

static void action_install_tickets_free_data(install_tickets_data* data) {
    util_free_contents(data->contents, data->installInfo.total);
    free(data);
}

static void action_install_tickets_done_onresponse(ui_view* view, void* data, bool response) {
    action_install_tickets_free_data((install_tickets_data*) data);

    prompt_destroy(view);
}

static void action_install_tickets_update(ui_view* view, void* data, float* progress, char* progressText) {
    install_tickets_data* installData = (install_tickets_data*) data;

    if(installData->installInfo.finished) {
        ui_pop();
        progressbar_destroy(view);

        if(installData->installInfo.premature) {
            action_install_tickets_free_data(installData);
        } else {
            ui_push(prompt_create("Success", "Install finished.", COLOR_TEXT, false, data, NULL, action_install_tickets_draw_top, action_install_tickets_done_onresponse));
        }

        return;
    }

    if(hidKeysDown() & KEY_B) {
        svcSignalEvent(installData->cancelEvent);
    }

    *progress = installData->installInfo.currTotal != 0 ? (float) ((double) installData->installInfo.currProcessed / (double) installData->installInfo.currTotal) : 0;
    snprintf(progressText, PROGRESS_TEXT_MAX, "%lu / %lu\n%.2f MB / %.2f MB", installData->installInfo.processed, installData->installInfo.total, installData->installInfo.currProcessed / 1024.0 / 1024.0, installData->installInfo.currTotal / 1024.0 / 1024.0);
}

static void action_install_tickets_onresponse(ui_view* view, void* data, bool response) {
    prompt_destroy(view);

    install_tickets_data* installData = (install_tickets_data*) data;

    if(response) {
        installData->cancelEvent = task_copy_data(&installData->installInfo);
        if(installData->cancelEvent != 0) {
            ui_view* progressView = progressbar_create("Installing ticket(s)", "Press B to cancel.", data, action_install_tickets_update, action_install_tickets_draw_top);
            snprintf(progressbar_get_progress_text(progressView), PROGRESS_TEXT_MAX, "0 / %lu", installData->installInfo.total);
            ui_push(progressView);
        } else {
            error_display(NULL, installData->base, ui_draw_file_info, "Failed to initiate ticket installation.");

            action_install_tickets_free_data(installData);
        }
    } else {
        action_install_tickets_free_data(installData);
    }
}

void action_install_tickets(file_info* info, bool* populated) {
    install_tickets_data* data = (install_tickets_data*) calloc(1, sizeof(install_tickets_data));
    data->base = info;

    data->installInfo.data = data;

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

    data->installInfo.resultError = action_install_tickets_result_error;
    data->installInfo.ioError = action_install_tickets_io_error;

    data->cancelEvent = 0;

    Result res = 0;
    if(R_FAILED(res = util_populate_contents(&data->contents, &data->installInfo.total, info->archive, info->path, false, false, ".tik", util_filter_file_extension))) {
        error_display_res(NULL, info, ui_draw_file_info, res, "Failed to retrieve content list.");

        free(data);
        return;
    }

    ui_push(prompt_create("Confirmation", "Install the selected ticket(s)?", COLOR_TEXT, true, data, NULL, action_install_tickets_draw_top, action_install_tickets_onresponse));
}