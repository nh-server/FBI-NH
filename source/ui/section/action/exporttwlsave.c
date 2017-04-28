#include <malloc.h>
#include <stdio.h>

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
#include "../../../core/spi.h"
#include "../../../core/util.h"

typedef struct {
    title_info* title;

    data_op_data exportInfo;
} export_twl_save_data;

static void action_export_twl_save_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    ui_draw_title_info(view, ((export_twl_save_data*) data)->title, x1, y1, x2, y2);
}

static Result action_export_twl_save_is_src_directory(void* data, u32 index, bool* isDirectory) {
    *isDirectory = false;
    return 0;
}

static Result action_export_twl_save_make_dst_directory(void* data, u32 index) {
    return 0;
}

static Result action_export_twl_save_open_src(void* data, u32 index, u32* handle) {
    return spi_init_card();
}

static Result action_export_twl_save_close_src(void* data, u32 index, bool succeeded, u32 handle) {
    return spi_deinit_card();
}

static Result action_export_twl_save_get_src_size(void* data, u32 handle, u64* size) {
    Result res = 0;

    u32 saveSize = 0;
    if(R_SUCCEEDED(res = spi_get_save_size(&saveSize))) {
        *size = saveSize;
    }

    return res;
}

static Result action_export_twl_save_read_src(void* data, u32 handle, u32* bytesRead, void* buffer, u64 offset, u32 size) {
    return spi_read_save(bytesRead, buffer, (u32) offset, size);
}

static Result action_export_twl_save_open_dst(void* data, u32 index, void* initialReadBlock, u64 size, u32* handle) {
    export_twl_save_data* exportData = (export_twl_save_data*) data;

    Result res = 0;

    FS_Archive sdmcArchive = 0;
    if(R_SUCCEEDED(res = FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, "")))) {
        if(R_SUCCEEDED(res = util_ensure_dir(sdmcArchive, "/fbi/")) && R_SUCCEEDED(res = util_ensure_dir(sdmcArchive, "/fbi/save/"))) {
            char gameName[0x10] = {'\0'};
            util_escape_file_name(gameName, exportData->title->productCode, sizeof(gameName));

            char path[FILE_PATH_MAX];
            snprintf(path, sizeof(path), "/fbi/save/%s.sav", gameName);

            FS_Path* fsPath = util_make_path_utf8(path);
            if(fsPath != NULL) {
                res = FSUSER_OpenFileDirectly(handle, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), *fsPath, FS_OPEN_WRITE | FS_OPEN_CREATE, 0);

                util_free_path_utf8(fsPath);
            } else {
                res = R_FBI_OUT_OF_MEMORY;
            }
        }

        FSUSER_CloseArchive(sdmcArchive);
    }

    return res;
}

static Result action_export_twl_save_close_dst(void* data, u32 index, bool succeeded, u32 handle) {
    return FSFILE_Close(handle);
}

static Result action_export_twl_save_write_dst(void* data, u32 handle, u32* bytesWritten, void* buffer, u64 offset, u32 size) {
    return FSFILE_Write(handle, bytesWritten, offset, buffer, size, 0);
}

static Result action_export_twl_save_suspend_copy(void* data, u32 index, u32* srcHandle, u32* dstHandle) {
    return 0;
}

static Result action_export_twl_save_restore_copy(void* data, u32 index, u32* srcHandle, u32* dstHandle) {
    return 0;
}

static Result action_export_twl_save_suspend(void* data, u32 index) {
    return 0;
}

static Result action_export_twl_save_restore(void* data, u32 index) {
    return 0;
}

static bool action_export_twl_save_error(void* data, u32 index, Result res) {
    export_twl_save_data* exportData = (export_twl_save_data*) data;

    if(res == R_FBI_CANCELLED) {
        prompt_display_notify("Failure", "Export cancelled.", COLOR_TEXT, exportData->title, ui_draw_title_info, NULL);
    } else {
        error_display_res(exportData->title, ui_draw_title_info, res, "Failed to export save.");
    }

    return false;
}

static void action_export_twl_save_update(ui_view* view, void* data, float* progress, char* text) {
    export_twl_save_data* exportData = (export_twl_save_data*) data;

    if(exportData->exportInfo.finished) {
        ui_pop();
        info_destroy(view);

        if(R_SUCCEEDED(exportData->exportInfo.result)) {
            prompt_display_notify("Success", "Save exported.", COLOR_TEXT, exportData->title, ui_draw_title_info, NULL);
        }

        free(data);

        return;
    }

    if(hidKeysDown() & KEY_B) {
        svcSignalEvent(exportData->exportInfo.cancelEvent);
    }

    *progress = exportData->exportInfo.currTotal != 0 ? (float) ((double) exportData->exportInfo.currProcessed / (double) exportData->exportInfo.currTotal) : 0;
    snprintf(text, PROGRESS_TEXT_MAX, "%.2f %s / %.2f %s\n%.2f %s/s", util_get_display_size(exportData->exportInfo.currProcessed), util_get_display_size_units(exportData->exportInfo.currProcessed), util_get_display_size(exportData->exportInfo.currTotal), util_get_display_size_units(exportData->exportInfo.currTotal), util_get_display_size(exportData->exportInfo.copyBytesPerSecond), util_get_display_size_units(exportData->exportInfo.copyBytesPerSecond));
}

static void action_export_twl_save_onresponse(ui_view* view, void* data, u32 response) {
    if(response == PROMPT_YES) {
        export_twl_save_data* exportData = (export_twl_save_data*) data;

        Result res = task_data_op(&exportData->exportInfo);
        if(R_SUCCEEDED(res)) {
            info_display("Exporting Save", "Press B to cancel.", true, data, action_export_twl_save_update, action_export_twl_save_draw_top);
        } else {
            error_display_res(exportData->title, ui_draw_title_info, res, "Failed to initiate save export.");
            free(data);
        }
    } else {
        free(data);
    }
}

void action_export_twl_save(linked_list* items, list_item* selected) {
    export_twl_save_data* data = (export_twl_save_data*) calloc(1, sizeof(export_twl_save_data));
    if(data == NULL) {
        error_display(NULL, NULL, "Failed to allocate export TWL save data.");

        return;
    }

    data->title = (title_info*) selected->data;

    data->exportInfo.data = data;

    data->exportInfo.op = DATAOP_COPY;

    data->exportInfo.copyBufferSize = 128 * 1024;
    data->exportInfo.copyEmpty = true;

    data->exportInfo.total = 1;

    data->exportInfo.isSrcDirectory = action_export_twl_save_is_src_directory;
    data->exportInfo.makeDstDirectory = action_export_twl_save_make_dst_directory;

    data->exportInfo.openSrc = action_export_twl_save_open_src;
    data->exportInfo.closeSrc = action_export_twl_save_close_src;
    data->exportInfo.getSrcSize = action_export_twl_save_get_src_size;
    data->exportInfo.readSrc = action_export_twl_save_read_src;

    data->exportInfo.openDst = action_export_twl_save_open_dst;
    data->exportInfo.closeDst = action_export_twl_save_close_dst;
    data->exportInfo.writeDst = action_export_twl_save_write_dst;

    data->exportInfo.suspendCopy = action_export_twl_save_suspend_copy;
    data->exportInfo.restoreCopy = action_export_twl_save_restore_copy;

    data->exportInfo.suspend = action_export_twl_save_suspend;
    data->exportInfo.restore = action_export_twl_save_restore;

    data->exportInfo.error = action_export_twl_save_error;

    data->exportInfo.finished = true;

    prompt_display_yes_no("Confirmation", "Export the save of the selected title?", COLOR_TEXT, data, action_export_twl_save_draw_top, action_export_twl_save_onresponse);
}