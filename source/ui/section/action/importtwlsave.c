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

    data_op_data importInfo;
} import_twl_save_data;

static void action_import_twl_save_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    ui_draw_title_info(view, ((import_twl_save_data*) data)->title, x1, y1, x2, y2);
}

static Result action_import_twl_save_is_src_directory(void* data, u32 index, bool* isDirectory) {
    *isDirectory = false;
    return 0;
}

static Result action_import_twl_save_make_dst_directory(void* data, u32 index) {
    return 0;
}

static Result action_import_twl_save_open_src(void* data, u32 index, u32* handle) {
    import_twl_save_data* importData = (import_twl_save_data*) data;

    Result res = 0;

    char gameName[0x10] = {'\0'};
    util_escape_file_name(gameName, importData->title->productCode, sizeof(gameName));

    char path[FILE_PATH_MAX];
    snprintf(path, sizeof(path), "/fbi/save/%s.sav", gameName);

    FS_Path* fsPath = util_make_path_utf8(path);
    if(fsPath != NULL) {
        res = FSUSER_OpenFileDirectly(handle, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), *fsPath, FS_OPEN_READ, 0);

        util_free_path_utf8(fsPath);
    } else {
        res = R_FBI_OUT_OF_MEMORY;
    }

    return res;
}

static Result action_import_twl_save_close_src(void* data, u32 index, bool succeeded, u32 handle) {
    return FSFILE_Close(handle);
}

static Result action_import_twl_save_get_src_size(void* data, u32 handle, u64* size) {
    return FSFILE_GetSize(handle, size);
}

static Result action_import_twl_save_read_src(void* data, u32 handle, u32* bytesRead, void* buffer, u64 offset, u32 size) {
    return FSFILE_Read(handle, bytesRead, offset, buffer, size);
}

static Result action_import_twl_save_open_dst(void* data, u32 index, void* initialReadBlock, u64 size, u32* handle) {
    return spi_init_card();
}

static Result action_import_twl_save_close_dst(void* data, u32 index, bool succeeded, u32 handle) {
    return spi_deinit_card();
}

static Result action_import_twl_save_write_dst(void* data, u32 handle, u32* bytesWritten, void* buffer, u64 offset, u32 size) {
    return spi_write_save(bytesWritten, buffer, (u32) offset, size);
}

static Result action_import_twl_save_suspend_copy(void* data, u32 index, u32* srcHandle, u32* dstHandle) {
    return 0;
}

static Result action_import_twl_save_restore_copy(void* data, u32 index, u32* srcHandle, u32* dstHandle) {
    return 0;
}

static Result action_import_twl_save_suspend(void* data, u32 index) {
    return 0;
}

static Result action_import_twl_save_restore(void* data, u32 index) {
    return 0;
}

static bool action_import_twl_save_error(void* data, u32 index, Result res) {
    import_twl_save_data* importData = (import_twl_save_data*) data;

    if(res == R_FBI_CANCELLED) {
        prompt_display_notify("Failure", "Import cancelled.", COLOR_TEXT, importData->title, ui_draw_title_info, NULL);
    } else {
        error_display_res(importData->title, ui_draw_title_info, res, "Failed to import save.");
    }

    return false;
}

static void action_import_twl_save_update(ui_view* view, void* data, float* progress, char* text) {
    import_twl_save_data* importData = (import_twl_save_data*) data;

    if(importData->importInfo.finished) {
        ui_pop();
        info_destroy(view);

        if(R_SUCCEEDED(importData->importInfo.result)) {
            prompt_display_notify("Success", "Save imported.", COLOR_TEXT, importData->title, ui_draw_title_info, NULL);
        }

        free(data);

        return;
    }

    if(hidKeysDown() & KEY_B) {
        svcSignalEvent(importData->importInfo.cancelEvent);
    }

    *progress = importData->importInfo.currTotal != 0 ? (float) ((double) importData->importInfo.currProcessed / (double) importData->importInfo.currTotal) : 0;
    snprintf(text, PROGRESS_TEXT_MAX, "%.2f %s / %.2f %s\n%.2f %s/s", util_get_display_size(importData->importInfo.currProcessed), util_get_display_size_units(importData->importInfo.currProcessed), util_get_display_size(importData->importInfo.currTotal), util_get_display_size_units(importData->importInfo.currTotal), util_get_display_size(importData->importInfo.copyBytesPerSecond), util_get_display_size_units(importData->importInfo.copyBytesPerSecond));
}

static void action_import_twl_save_onresponse(ui_view* view, void* data, u32 response) {
    if(response == PROMPT_YES) {
        import_twl_save_data* importData = (import_twl_save_data*) data;

        Result res = task_data_op(&importData->importInfo);
        if(R_SUCCEEDED(res)) {
            info_display("Importing Save", "Press B to cancel.", true, data, action_import_twl_save_update, action_import_twl_save_draw_top);
        } else {
            error_display_res(importData->title, ui_draw_title_info, res, "Failed to initiate save import.");
            free(data);
        }
    } else {
        free(data);
    }
}

void action_import_twl_save(linked_list* items, list_item* selected) {
    import_twl_save_data* data = (import_twl_save_data*) calloc(1, sizeof(import_twl_save_data));
    if(data == NULL) {
        error_display(NULL, NULL, "Failed to allocate import TWL save data.");

        return;
    }

    data->title = (title_info*) selected->data;

    data->importInfo.data = data;

    data->importInfo.op = DATAOP_COPY;

    data->importInfo.copyBufferSize = 16 * 1024;
    data->importInfo.copyEmpty = true;

    data->importInfo.total = 1;

    data->importInfo.isSrcDirectory = action_import_twl_save_is_src_directory;
    data->importInfo.makeDstDirectory = action_import_twl_save_make_dst_directory;

    data->importInfo.openSrc = action_import_twl_save_open_src;
    data->importInfo.closeSrc = action_import_twl_save_close_src;
    data->importInfo.getSrcSize = action_import_twl_save_get_src_size;
    data->importInfo.readSrc = action_import_twl_save_read_src;

    data->importInfo.openDst = action_import_twl_save_open_dst;
    data->importInfo.closeDst = action_import_twl_save_close_dst;
    data->importInfo.writeDst = action_import_twl_save_write_dst;

    data->importInfo.suspendCopy = action_import_twl_save_suspend_copy;
    data->importInfo.restoreCopy = action_import_twl_save_restore_copy;

    data->importInfo.suspend = action_import_twl_save_suspend;
    data->importInfo.restore = action_import_twl_save_restore;

    data->importInfo.error = action_import_twl_save_error;

    data->importInfo.finished = true;

    prompt_display_yes_no("Confirmation", "Import the save of the selected title?", COLOR_TEXT, data, action_import_twl_save_draw_top, action_import_twl_save_onresponse);
}