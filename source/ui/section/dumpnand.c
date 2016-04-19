#include <malloc.h>
#include <stdio.h>

#include <3ds.h>

#include "task/task.h"
#include "../error.h"
#include "../progressbar.h"
#include "../prompt.h"
#include "../../screen.h"

typedef struct {
    copy_data_info dumpInfo;
    Handle cancelEvent;
} dump_nand_data;

Result dumpnand_is_src_directory(void* data, u32 index, bool* isDirectory) {
    *isDirectory = false;
    return 0;
}

Result dumpnand_make_dst_directory(void* data, u32 index) {
    return 0;
}

Result dumpnand_open_src(void* data, u32 index, u32* handle) {
    FS_Archive wnandArchive = {ARCHIVE_NAND_W_FS, fsMakePath(PATH_EMPTY, "")};
    return FSUSER_OpenFileDirectly(handle, wnandArchive, fsMakePath(PATH_UTF16, u"/"), FS_OPEN_READ, 0);
}

Result dumpnand_close_src(void* data, u32 index, bool succeeded, u32 handle) {
    return FSFILE_Close(handle);
}

Result dumpnand_get_src_size(void* data, u32 handle, u64* size) {
    return FSFILE_GetSize(handle, size);
}

Result dumpnand_read_src(void* data, u32 handle, u32* bytesRead, void* buffer, u64 offset, u32 size) {
    return FSFILE_Read(handle, bytesRead, offset, buffer, size);
}

Result dumpnand_open_dst(void* data, u32 index, void* initialReadBlock, u32* handle) {
    FS_Archive sdmcArchive = {ARCHIVE_SDMC, {PATH_BINARY, 0, (u8*) ""}};
    return FSUSER_OpenFileDirectly(handle, sdmcArchive, fsMakePath(PATH_ASCII, "/NAND.bin"), FS_OPEN_WRITE | FS_OPEN_CREATE, 0);
}

Result dumpnand_close_dst(void* data, u32 index, bool succeeded, u32 handle) {
    return FSFILE_Close(handle);
}

Result dumpnand_write_dst(void* data, u32 handle, u32* bytesWritten, void* buffer, u64 offset, u32 size) {
    return FSFILE_Write(handle, bytesWritten, offset, buffer, size, 0);
}

bool dumpnand_result_error(void* data, u32 index, Result res) {
    if(res == MAKERESULT(RL_PERMANENT, RS_CANCELED, RM_APPLICATION, RD_CANCEL_REQUESTED)) {
        ui_push(prompt_create("Failure", "Dump cancelled.", COLOR_TEXT, false, NULL, NULL, NULL, NULL));
    } else {
        error_display_res(NULL, NULL, NULL, res, "Failed to dump NAND.");
    }

    return false;
}

bool dumpnand_io_error(void* data, u32 index, int err) {
    error_display_errno(NULL, NULL, NULL, err, "Failed to dump NAND.");
    return false;
}

static void dumpnand_done_onresponse(ui_view* view, void* data, bool response) {
    prompt_destroy(view);
}

static void dumpnand_update(ui_view* view, void* data, float* progress, char* progressText) {
    dump_nand_data* dumpData = (dump_nand_data*) data;

    if(dumpData->dumpInfo.finished) {
        ui_pop();
        progressbar_destroy(view);

        if(!dumpData->dumpInfo.premature) {
            ui_push(prompt_create("Success", "NAND dumped.", COLOR_TEXT, false, NULL, NULL, NULL, dumpnand_done_onresponse));
        }

        free(dumpData);

        return;
    }

    if(hidKeysDown() & KEY_B) {
        svcSignalEvent(dumpData->cancelEvent);
    }

    *progress = dumpData->dumpInfo.currTotal != 0 ? (float) ((double) dumpData->dumpInfo.currProcessed / (double) dumpData->dumpInfo.currTotal) : 0;
    snprintf(progressText, PROGRESS_TEXT_MAX, "%.2f MB / %.2f MB", dumpData->dumpInfo.currProcessed / 1024.0f / 1024.0f, dumpData->dumpInfo.currTotal / 1024.0f / 1024.0f);
}

static void dumpnand_onresponse(ui_view* view, void* data, bool response) {
    prompt_destroy(view);

    if(response) {
        dump_nand_data* dumpData = (dump_nand_data*) data;

        dumpData->cancelEvent = task_copy_data(&dumpData->dumpInfo);
        if(dumpData->cancelEvent != 0) {
            ui_push(progressbar_create("Dumping NAND", "Press B to cancel.", data, dumpnand_update, NULL));
        } else {
            error_display(NULL, NULL, NULL, "Failed to initiate NAND dump.");
        }
    } else {
        free(data);
    }
}

void dump_nand() {
    dump_nand_data* data = (dump_nand_data*) calloc(1, sizeof(dump_nand_data));

    data->dumpInfo.data = data;

    data->dumpInfo.copyEmpty = true;

    data->dumpInfo.total = 1;

    data->dumpInfo.isSrcDirectory = dumpnand_is_src_directory;
    data->dumpInfo.makeDstDirectory = dumpnand_make_dst_directory;

    data->dumpInfo.openSrc = dumpnand_open_src;
    data->dumpInfo.closeSrc = dumpnand_close_src;
    data->dumpInfo.getSrcSize = dumpnand_get_src_size;
    data->dumpInfo.readSrc = dumpnand_read_src;

    data->dumpInfo.openDst = dumpnand_open_dst;
    data->dumpInfo.closeDst = dumpnand_close_dst;
    data->dumpInfo.writeDst = dumpnand_write_dst;

    data->dumpInfo.resultError = dumpnand_result_error;
    data->dumpInfo.ioError = dumpnand_io_error;

    data->cancelEvent = 0;

    ui_push(prompt_create("Confirmation", "Dump raw NAND image to the SD card?", COLOR_TEXT, true, data, NULL, NULL, dumpnand_onresponse));
}