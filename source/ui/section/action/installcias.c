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
    bool delete;
    bool* populated;
    char** contents;

    u64 currTitleId;

    data_op_info installInfo;
    Handle cancelEvent;
} install_cias_data;

static Result action_install_cias_is_src_directory(void* data, u32 index, bool* isDirectory) {
    *isDirectory = false;
    return 0;
}

static Result action_install_cias_make_dst_directory(void* data, u32 index) {
    return 0;
}

static Result action_install_cias_open_src(void* data, u32 index, u32* handle) {
    install_cias_data* installData = (install_cias_data*) data;

    FS_Path* fsPath = util_make_path_utf8(installData->contents[index]);

    Result res = FSUSER_OpenFile(handle, *installData->base->archive, *fsPath, FS_OPEN_READ, 0);

    util_free_path_utf8(fsPath);

    return res;
}

static Result action_install_cias_close_src(void* data, u32 index, bool succeeded, u32 handle) {
    install_cias_data* installData = (install_cias_data*) data;

    Result res = 0;
    if(R_SUCCEEDED(res = FSFILE_Close(handle)) && installData->delete && succeeded) {
        FS_Path* fsPath = util_make_path_utf8(installData->contents[index]);

        FSUSER_DeleteFile(*installData->base->archive, *fsPath);

        util_free_path_utf8(fsPath);
    }

    return res;
}

static Result action_install_cias_get_src_size(void* data, u32 handle, u64* size) {
    return FSFILE_GetSize(handle, size);
}

static Result action_install_cias_read_src(void* data, u32 handle, u32* bytesRead, void* buffer, u64 offset, u32 size) {
    return FSFILE_Read(handle, bytesRead, offset, buffer, size);
}

static Result action_install_cias_open_dst(void* data, u32 index, void* initialReadBlock, u32* handle) {
    install_cias_data* installData = (install_cias_data*) data;

    u8* buffer = (u8*) initialReadBlock;

    u32 headerSize = *(u32*) &buffer[0x00];
    u32 certSize = *(u32*) &buffer[0x08];
    u64 titleId = __builtin_bswap64(*(u64*) &buffer[((headerSize + 0x3F) & ~0x3F) + ((certSize + 0x3F) & ~0x3F) + 0x1DC]);

    FS_MediaType dest = ((titleId >> 32) & 0x8010) != 0 ? MEDIATYPE_NAND : MEDIATYPE_SD;

    u8 n3ds = false;
    if(R_SUCCEEDED(APT_CheckNew3DS(&n3ds)) && !n3ds && ((titleId >> 28) & 0xF) == 2) {
        return MAKERESULT(RL_PERMANENT, RS_NOTSUPPORTED, RM_APPLICATION, RD_INVALID_COMBINATION);
    }

    // Deleting FBI before it reinstalls itself causes issues.
    if(((titleId >> 8) & 0xFFFFF) != 0xF8001) {
        AM_DeleteTitle(dest, titleId);
        AM_DeleteTicket(titleId);

        if(dest == 1) {
            AM_QueryAvailableExternalTitleDatabase(NULL);
        }
    }

    Result res = AM_StartCiaInstall(dest, handle);
    if(R_SUCCEEDED(res)) {
        installData->currTitleId = titleId;
    }

    return res;
}

static Result action_install_cias_close_dst(void* data, u32 index, bool succeeded, u32 handle) {
    if(succeeded) {
        install_cias_data* installData = (install_cias_data*) data;

        Result res = 0;
        if(R_SUCCEEDED(res = AM_FinishCiaInstall(handle))) {
            if(installData->currTitleId == 0x0004013800000002 || installData->currTitleId == 0x0004013820000002) {
                res = AM_InstallFirm(installData->currTitleId);
            }
        }

        return res;
    } else {
        return AM_CancelCIAInstall(handle);
    }
}

static Result action_install_cias_write_dst(void* data, u32 handle, u32* bytesWritten, void* buffer, u64 offset, u32 size) {
    return FSFILE_Write(handle, bytesWritten, offset, buffer, size, 0);
}

bool action_install_cias_result_error(void* data, u32 index, Result res) {
    install_cias_data* installData = (install_cias_data*) data;

    if(res == MAKERESULT(RL_PERMANENT, RS_CANCELED, RM_APPLICATION, RD_CANCEL_REQUESTED)) {
        ui_push(prompt_create("Failure", "Install cancelled.", COLOR_TEXT, false, installData->base, NULL, ui_draw_file_info, NULL));
        return false;
    } else {
        char* path = installData->contents[index];

        volatile bool dismissed = false;
        if(res == MAKERESULT(RL_PERMANENT, RS_NOTSUPPORTED, RM_APPLICATION, RD_INVALID_COMBINATION)) {
            if(strlen(path) > 48) {
                error_display(&dismissed, installData->base, ui_draw_file_info, "Failed to install CIA file.\n%.45s...\nAttempted to install N3DS title to O3DS.", path);
            } else {
                error_display(&dismissed, installData->base, ui_draw_file_info, "Failed to install CIA file.\n%.48s\nAttempted to install N3DS title to O3DS.", path);
            }
        } else {
            if(strlen(path) > 48) {
                error_display_res(&dismissed, installData->base, ui_draw_file_info, res, "Failed to install CIA file.\n%.45s...", path);
            } else {
                error_display_res(&dismissed, installData->base, ui_draw_file_info, res, "Failed to install CIA file.\n%.48s", path);
            }
        }

        while(!dismissed) {
            svcSleepThread(1000000);
        }
    }

    return index < installData->installInfo.total - 1;
}

bool action_install_cias_io_error(void* data, u32 index, int err) {
    install_cias_data* installData = (install_cias_data*) data;

    char* path = installData->contents[index];

    volatile bool dismissed = false;
    if(strlen(path) > 48) {
        error_display_errno(&dismissed, installData->base, ui_draw_file_info, err, "Failed to install CIA file.\n%.45s...", path);
    } else {
        error_display_errno(&dismissed, installData->base, ui_draw_file_info, err, "Failed to install CIA file.\n%.48s", path);
    }

    while(!dismissed) {
        svcSleepThread(1000000);
    }

    return index < installData->installInfo.total - 1;
}

static void action_install_cias_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    ui_draw_file_info(view, ((install_cias_data*) data)->base, x1, y1, x2, y2);
}

static void action_install_cias_free_data(install_cias_data* data) {
    util_free_contents(data->contents, data->installInfo.total);
    free(data);
}

static void action_install_cias_done_onresponse(ui_view* view, void* data, bool response) {
    action_install_cias_free_data((install_cias_data*) data);

    prompt_destroy(view);
}

static void action_install_cias_update(ui_view* view, void* data, float* progress, char* progressText) {
    install_cias_data* installData = (install_cias_data*) data;

    if(installData->installInfo.finished) {
        if(installData->delete) {
            *installData->populated = false;
        }

        ui_pop();
        progressbar_destroy(view);

        if(installData->installInfo.premature) {
            action_install_cias_free_data(installData);
        } else {
            ui_push(prompt_create("Success", "Install finished.", COLOR_TEXT, false, data, NULL, action_install_cias_draw_top, action_install_cias_done_onresponse));
        }

        return;
    }

    if(hidKeysDown() & KEY_B) {
        svcSignalEvent(installData->cancelEvent);
    }

    *progress = installData->installInfo.currTotal != 0 ? (float) ((double) installData->installInfo.currProcessed / (double) installData->installInfo.currTotal) : 0;
    snprintf(progressText, PROGRESS_TEXT_MAX, "%lu / %lu\n%.2f MB / %.2f MB", installData->installInfo.processed, installData->installInfo.total, installData->installInfo.currProcessed / 1024.0 / 1024.0, installData->installInfo.currTotal / 1024.0 / 1024.0);
}

static void action_install_cias_onresponse(ui_view* view, void* data, bool response) {
    prompt_destroy(view);

    install_cias_data* installData = (install_cias_data*) data;

    if(response) {
        installData->cancelEvent = task_data_op(&installData->installInfo);
        if(installData->cancelEvent != 0) {
            ui_view* progressView = progressbar_create("Installing CIA(s)", "Press B to cancel.", data, action_install_cias_update, action_install_cias_draw_top);
            snprintf(progressbar_get_progress_text(progressView), PROGRESS_TEXT_MAX, "0 / %lu", installData->installInfo.total);
            ui_push(progressView);
        } else {
            error_display(NULL, installData->base, ui_draw_file_info, "Failed to initiate CIA installation.");

            action_install_cias_free_data(installData);
        }
    } else {
        action_install_cias_free_data(installData);
    }
}

static void action_install_cias_internal(file_info* info, bool* populated, bool delete) {
    install_cias_data* data = (install_cias_data*) calloc(1, sizeof(install_cias_data));
    data->base = info;
    data->delete = delete;
    data->populated = populated;

    data->currTitleId = 0;

    data->installInfo.data = data;

    data->installInfo.op = DATAOP_COPY;

    data->installInfo.copyEmpty = false;

    data->installInfo.isSrcDirectory = action_install_cias_is_src_directory;
    data->installInfo.makeDstDirectory = action_install_cias_make_dst_directory;

    data->installInfo.openSrc = action_install_cias_open_src;
    data->installInfo.closeSrc = action_install_cias_close_src;
    data->installInfo.getSrcSize = action_install_cias_get_src_size;
    data->installInfo.readSrc = action_install_cias_read_src;

    data->installInfo.openDst = action_install_cias_open_dst;
    data->installInfo.closeDst = action_install_cias_close_dst;
    data->installInfo.writeDst = action_install_cias_write_dst;

    data->installInfo.resultError = action_install_cias_result_error;
    data->installInfo.ioError = action_install_cias_io_error;

    data->cancelEvent = 0;

    Result res = 0;
    if(R_FAILED(res = util_populate_contents(&data->contents, &data->installInfo.total, info->archive, info->path, false, false, ".cia", util_filter_file_extension))) {
        error_display_res(NULL, info, ui_draw_file_info, res, "Failed to retrieve content list.");

        free(data);
        return;
    }

    ui_push(prompt_create("Confirmation", "Install the selected CIA(s)?", COLOR_TEXT, true, data, NULL, action_install_cias_draw_top, action_install_cias_onresponse));
}

void action_install_cias(file_info* info, bool* populated) {
    action_install_cias_internal(info, populated, false);
}

void action_install_cias_delete(file_info* info, bool* populated) {
    action_install_cias_internal(info, populated, true);
}