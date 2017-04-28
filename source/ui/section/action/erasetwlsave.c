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
#include "../../../core/spi.h"
#include "../../../core/util.h"

typedef struct {
    title_info* title;

    data_op_data eraseInfo;
} erase_twl_save_data;

static void action_erase_twl_save_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    ui_draw_title_info(view, ((erase_twl_save_data*) data)->title, x1, y1, x2, y2);
}

static Result action_erase_twl_save_is_src_directory(void* data, u32 index, bool* isDirectory) {
    *isDirectory = false;
    return 0;
}

static Result action_erase_twl_save_make_dst_directory(void* data, u32 index) {
    return 0;
}

static Result action_erase_twl_save_open_src(void* data, u32 index, u32* handle) {
    return 0;
}

static Result action_erase_twl_save_close_src(void* data, u32 index, bool succeeded, u32 handle) {
    return 0;
}

static Result action_erase_twl_save_get_src_size(void* data, u32 handle, u64* size) {
    Result res = 0;

    u32 saveSize = 0;
    if(R_SUCCEEDED(res = spi_init_card()) && R_SUCCEEDED(res = spi_get_save_size(&saveSize)) && R_SUCCEEDED(res = spi_deinit_card())) {
        *size = saveSize;
    }

    return res;
}

static Result action_erase_twl_save_read_src(void* data, u32 handle, u32* bytesRead, void* buffer, u64 offset, u32 size) {
    memset(buffer, 0, size);
    *bytesRead = size;

    return 0;
}

static Result action_erase_twl_save_open_dst(void* data, u32 index, void* initialReadBlock, u64 size, u32* handle) {
    return spi_init_card();
}

static Result action_erase_twl_save_close_dst(void* data, u32 index, bool succeeded, u32 handle) {
    return spi_deinit_card();
}

static Result action_erase_twl_save_write_dst(void* data, u32 handle, u32* bytesWritten, void* buffer, u64 offset, u32 size) {
    return spi_write_save(bytesWritten, buffer, (u32) offset, size);
}

static Result action_erase_twl_save_suspend_copy(void* data, u32 index, u32* srcHandle, u32* dstHandle) {
    return 0;
}

static Result action_erase_twl_save_restore_copy(void* data, u32 index, u32* srcHandle, u32* dstHandle) {
    return 0;
}

static Result action_erase_twl_save_suspend(void* data, u32 index) {
    return 0;
}

static Result action_erase_twl_save_restore(void* data, u32 index) {
    return 0;
}

static bool action_erase_twl_save_error(void* data, u32 index, Result res) {
    erase_twl_save_data* eraseData = (erase_twl_save_data*) data;

    if(res == R_FBI_CANCELLED) {
        prompt_display_notify("Failure", "Erase cancelled.", COLOR_TEXT, eraseData->title, ui_draw_title_info, NULL);
    } else {
        error_display_res(eraseData->title, ui_draw_title_info, res, "Failed to erase save.");
    }

    return false;
}

static void action_erase_twl_save_update(ui_view* view, void* data, float* progress, char* text) {
    erase_twl_save_data* eraseData = (erase_twl_save_data*) data;

    if(eraseData->eraseInfo.finished) {
        ui_pop();
        info_destroy(view);

        if(R_SUCCEEDED(eraseData->eraseInfo.result)) {
            prompt_display_notify("Success", "Save erased.", COLOR_TEXT, eraseData->title, ui_draw_title_info, NULL);
        }

        free(data);

        return;
    }

    if(hidKeysDown() & KEY_B) {
        svcSignalEvent(eraseData->eraseInfo.cancelEvent);
    }

    *progress = eraseData->eraseInfo.currTotal != 0 ? (float) ((double) eraseData->eraseInfo.currProcessed / (double) eraseData->eraseInfo.currTotal) : 0;
    snprintf(text, PROGRESS_TEXT_MAX, "%.2f %s / %.2f %s\n%.2f %s/s", util_get_display_size(eraseData->eraseInfo.currProcessed), util_get_display_size_units(eraseData->eraseInfo.currProcessed), util_get_display_size(eraseData->eraseInfo.currTotal), util_get_display_size_units(eraseData->eraseInfo.currTotal), util_get_display_size(eraseData->eraseInfo.copyBytesPerSecond), util_get_display_size_units(eraseData->eraseInfo.copyBytesPerSecond));
}

static void action_erase_twl_save_onresponse(ui_view* view, void* data, u32 response) {
    if(response == PROMPT_YES) {
        erase_twl_save_data* eraseData = (erase_twl_save_data*) data;

        Result res = task_data_op(&eraseData->eraseInfo);
        if(R_SUCCEEDED(res)) {
            info_display("Erasing Save", "Press B to cancel.", true, data, action_erase_twl_save_update, action_erase_twl_save_draw_top);
        } else {
            error_display_res(eraseData->title, ui_draw_title_info, res, "Failed to initiate save erase.");
            free(data);
        }
    } else {
        free(data);
    }
}

void action_erase_twl_save(linked_list* items, list_item* selected) {
    erase_twl_save_data* data = (erase_twl_save_data*) calloc(1, sizeof(erase_twl_save_data));
    if(data == NULL) {
        error_display(NULL, NULL, "Failed to allocate erase TWL save data.");

        return;
    }

    data->title = (title_info*) selected->data;

    data->eraseInfo.data = data;

    data->eraseInfo.op = DATAOP_COPY;

    data->eraseInfo.copyBufferSize = 16 * 1024;
    data->eraseInfo.copyEmpty = true;

    data->eraseInfo.total = 1;

    data->eraseInfo.isSrcDirectory = action_erase_twl_save_is_src_directory;
    data->eraseInfo.makeDstDirectory = action_erase_twl_save_make_dst_directory;

    data->eraseInfo.openSrc = action_erase_twl_save_open_src;
    data->eraseInfo.closeSrc = action_erase_twl_save_close_src;
    data->eraseInfo.getSrcSize = action_erase_twl_save_get_src_size;
    data->eraseInfo.readSrc = action_erase_twl_save_read_src;

    data->eraseInfo.openDst = action_erase_twl_save_open_dst;
    data->eraseInfo.closeDst = action_erase_twl_save_close_dst;
    data->eraseInfo.writeDst = action_erase_twl_save_write_dst;

    data->eraseInfo.suspendCopy = action_erase_twl_save_suspend_copy;
    data->eraseInfo.restoreCopy = action_erase_twl_save_restore_copy;

    data->eraseInfo.suspend = action_erase_twl_save_suspend;
    data->eraseInfo.restore = action_erase_twl_save_restore;

    data->eraseInfo.error = action_erase_twl_save_error;

    data->eraseInfo.finished = true;

    prompt_display_yes_no("Confirmation", "Erase the save of the selected title?", COLOR_TEXT, data, action_erase_twl_save_draw_top, action_erase_twl_save_onresponse);
}