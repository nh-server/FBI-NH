#include <malloc.h>
#include <stdio.h>

#include <3ds.h>

#include "../error.h"
#include "../progressbar.h"
#include "../prompt.h"

typedef struct {
    Handle in;
    Handle out;
    u64 offset;
    u64 size;

    u8 buffer[1024 * 1024];
} dump_nand_data;

static void dumpnand_done_onresponse(ui_view* view, void* data, bool response) {
    prompt_destroy(view);
}

static void dumpnand_update(ui_view* view, void* data, float* progress, char* progressText) {
    dump_nand_data* dumpData = (dump_nand_data*) data;

    if(hidKeysDown() & KEY_B) {
        if(dumpData->in != 0) {
            FSFILE_Close(dumpData->in);
            dumpData->in = 0;
        }

        if(dumpData->out != 0) {
            FSFILE_Close(dumpData->out);
            dumpData->out = 0;
        }

        progressbar_destroy(view);
        ui_pop();

        ui_push(prompt_create("Failure", "Dump cancelled.", 0xFF000000, false, data, NULL, NULL, dumpnand_done_onresponse));

        free(data);

        return;
    }

    Result res = 0;

    u32 bytesRead = 0;
    u32 bytesWritten = 0;
    if(R_SUCCEEDED(res = FSFILE_Read(dumpData->in, &bytesRead, dumpData->offset, dumpData->buffer, sizeof(dumpData->buffer))) && R_SUCCEEDED(res = FSFILE_Write(dumpData->out, &bytesWritten, dumpData->offset, dumpData->buffer, bytesRead, 0)) && bytesRead == bytesWritten) {
        dumpData->offset += bytesRead;
    }

    if(R_FAILED(res)) {
        if(dumpData->in != 0) {
            FSFILE_Close(dumpData->in);
            dumpData->in = 0;
        }

        if(dumpData->out != 0) {
            FSFILE_Close(dumpData->out);
            dumpData->out = 0;
        }

        progressbar_destroy(view);
        ui_pop();

        error_display_res(NULL, NULL, res, "Failed to dump NAND.");

        free(data);
    } else if(bytesRead != bytesWritten) {
        if(dumpData->in != 0) {
            FSFILE_Close(dumpData->in);
            dumpData->in = 0;
        }

        if(dumpData->out != 0) {
            FSFILE_Close(dumpData->out);
            dumpData->out = 0;
        }

        progressbar_destroy(view);
        ui_pop();

        error_display(NULL, NULL, "Failed to dump NAND: Read/Write size mismatch.");

        free(data);
    } else if(dumpData->offset >= dumpData->size) {
        if(dumpData->in != 0) {
            FSFILE_Close(dumpData->in);
            dumpData->in = 0;
        }

        if(dumpData->out != 0) {
            FSFILE_Close(dumpData->out);
            dumpData->out = 0;
        }

        progressbar_destroy(view);
        ui_pop();

        ui_push(prompt_create("Success", "NAND dumped.", 0xFF000000, false, NULL, NULL, NULL, dumpnand_done_onresponse));

        free(data);
    } else {
        *progress = (float) dumpData->offset / (float) dumpData->size;
        snprintf(progressText, PROGRESS_TEXT_MAX, "%.2f MB / %.2f MB", dumpData->offset / 1024.0f / 1024.0f, dumpData->size / 1024.0f / 1024.0f);
    }
}

static void dumpnand_onresponse(ui_view* view, void* data, bool response) {
    prompt_destroy(view);

    if(response) {
        dump_nand_data* dumpData = (dump_nand_data*) data;

        Result res = 0;

        FS_Archive wnandArchive = {ARCHIVE_NAND_W_FS, fsMakePath(PATH_EMPTY, "")};
        if(R_SUCCEEDED(res = FSUSER_OpenFileDirectly(&dumpData->in, wnandArchive, fsMakePath(PATH_UTF16, u"/"), FS_OPEN_READ, 0)) && R_SUCCEEDED(res = FSFILE_GetSize(dumpData->in, &dumpData->size))) {
            FS_Archive sdmcArchive = {ARCHIVE_SDMC, {PATH_BINARY, 0, (u8*) ""}};
            if(R_SUCCEEDED(res = FSUSER_OpenFileDirectly(&dumpData->out, sdmcArchive, fsMakePath(PATH_ASCII, "/NAND.bin"), FS_OPEN_CREATE | FS_OPEN_WRITE, 0))) {
                dumpData->offset = 0;

                ui_push(progressbar_create("Dumping NAND", "Press B to cancel.", data, dumpnand_update, NULL));
            }
        }

        if(R_FAILED(res)) {
            if(dumpData->in != 0) {
                FSFILE_Close(dumpData->in);
                dumpData->in = 0;
            }

            if(dumpData->out != 0) {
                FSFILE_Close(dumpData->out);
                dumpData->out = 0;
            }

            error_display_res(NULL, NULL, res, "Failed to prepare for NAND dump.");

            free(data);
        }
    } else {
        free(data);
    }
}

void dump_nand() {
    dump_nand_data* data = (dump_nand_data*) calloc(1, sizeof(dump_nand_data));
    data->in = 0;
    data->out = 0;
    data->offset = 0;
    data->size = 0;

    ui_push(prompt_create("Confirmation", "Dump raw NAND image to the SD card?", 0xFF000000, true, data, NULL, NULL, dumpnand_onresponse));
}