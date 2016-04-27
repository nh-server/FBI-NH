#include <malloc.h>
#include <stdio.h>

#include <3ds.h>

#include "section.h"
#include "task/task.h"
#include "../error.h"
#include "../info.h"
#include "../prompt.h"
#include "../ui.h"
#include "../../core/screen.h"

typedef struct {
    data_op_info dumpInfo;
    Handle cancelEvent;
} dump_nand_data;

static Result dumpnand_is_src_directory(void* data, u32 index, bool* isDirectory) {
    *isDirectory = false;
    return 0;
}

static Result dumpnand_make_dst_directory(void* data, u32 index) {
    return 0;
}

static Result dumpnand_open_src(void* data, u32 index, u32* handle) {
    FS_Archive wnandArchive = {ARCHIVE_NAND_W_FS, fsMakePath(PATH_EMPTY, "")};
    return FSUSER_OpenFileDirectly(handle, wnandArchive, fsMakePath(PATH_UTF16, u"/"), FS_OPEN_READ, 0);
}

static Result dumpnand_close_src(void* data, u32 index, bool succeeded, u32 handle) {
    return FSFILE_Close(handle);
}

static Result dumpnand_get_src_size(void* data, u32 handle, u64* size) {
    return FSFILE_GetSize(handle, size);
}

static Result dumpnand_read_src(void* data, u32 handle, u32* bytesRead, void* buffer, u64 offset, u32 size) {
    return FSFILE_Read(handle, bytesRead, offset, buffer, size);
}

static Result dumpnand_open_dst(void* data, u32 index, void* initialReadBlock, u32* handle) {
    FS_Archive sdmcArchive = {ARCHIVE_SDMC, {PATH_BINARY, 0, (u8*) ""}};
    return FSUSER_OpenFileDirectly(handle, sdmcArchive, fsMakePath(PATH_UTF16, u"/NAND.bin"), FS_OPEN_WRITE | FS_OPEN_CREATE, 0);
}

static Result dumpnand_close_dst(void* data, u32 index, bool succeeded, u32 handle) {
    return FSFILE_Close(handle);
}

static Result dumpnand_write_dst(void* data, u32 handle, u32* bytesWritten, void* buffer, u64 offset, u32 size) {
    return FSFILE_Write(handle, bytesWritten, offset, buffer, size, 0);
}

static bool dumpnand_error(void* data, u32 index, Result res) {
    if(res == R_FBI_CANCELLED) {
        prompt_display("Failure", "Dump cancelled.", COLOR_TEXT, false, NULL, NULL, NULL, NULL);
    } else {
        error_display_res(NULL, NULL, NULL, res, "Failed to dump NAND.");
    }

    return false;
}

static void dumpnand_update(ui_view* view, void* data, float* progress, char* text) {
    dump_nand_data* dumpData = (dump_nand_data*) data;

    if(dumpData->dumpInfo.finished) {
        ui_pop();
        info_destroy(view);

        if(!dumpData->dumpInfo.premature) {
            prompt_display("Success", "NAND dumped.", COLOR_TEXT, false, NULL, NULL, NULL, NULL);
        }

        free(dumpData);

        return;
    }

    if(hidKeysDown() & KEY_B) {
        svcSignalEvent(dumpData->cancelEvent);
    }

    *progress = dumpData->dumpInfo.currTotal != 0 ? (float) ((double) dumpData->dumpInfo.currProcessed / (double) dumpData->dumpInfo.currTotal) : 0;
    snprintf(text, PROGRESS_TEXT_MAX, "%.2f MB / %.2f MB", dumpData->dumpInfo.currProcessed / 1024.0f / 1024.0f, dumpData->dumpInfo.currTotal / 1024.0f / 1024.0f);
}

static void dumpnand_onresponse(ui_view* view, void* data, bool response) {
    if(response) {
        dump_nand_data* dumpData = (dump_nand_data*) data;

        dumpData->cancelEvent = task_data_op(&dumpData->dumpInfo);
        if(dumpData->cancelEvent != 0) {
            info_display("Dumping NAND", "Press B to cancel.", true, data, dumpnand_update, NULL);
        } else {
            error_display(NULL, NULL, NULL, "Failed to initiate NAND dump.");
        }
    } else {
        free(data);
    }
}

void dumpnand_open() {
    dump_nand_data* data = (dump_nand_data*) calloc(1, sizeof(dump_nand_data));
    if(data == NULL) {
        error_display(NULL, NULL, NULL, "Failed to allocate dump NAND data.");

        return;
    }

    data->dumpInfo.data = data;

    data->dumpInfo.op = DATAOP_COPY;

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

    data->dumpInfo.error = dumpnand_error;

    data->cancelEvent = 0;

    prompt_display("Confirmation", "Dump raw NAND image to the SD card?", COLOR_TEXT, true, data, NULL, NULL, dumpnand_onresponse);
}