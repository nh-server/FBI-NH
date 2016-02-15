#include <stdio.h>

#include <3ds.h>

#include "action.h"
#include "../../error.h"
#include "../../progressbar.h"
#include "../../prompt.h"
#include "../../../util.h"

static void action_export_secure_value_end_onresponse(ui_view* view, void* data, bool response) {
    prompt_destroy(view);
}

static void action_export_secure_value_update(ui_view* view, void* data, float* progress, char* progressText) {
    title_info* info = (title_info*) data;

    Result res = 0;

    bool exists = false;
    u64 value = 0;
    if(R_SUCCEEDED(res = FSUSER_GetSaveDataSecureValue(&exists, &value, SECUREVALUE_SLOT_SD, (u32) ((info->titleId >> 8) & 0xFFFFF), (u8) (info->titleId & 0xFF)))) {
        if(!exists) {
            ui_push(prompt_create("Failure", "Secure value not set.", 0xFF000000, false, info, NULL, ui_draw_title_info, action_export_secure_value_end_onresponse));

            progressbar_destroy(view);
            ui_pop();

            return;
        }

        FS_Archive sdmcArchive = {ARCHIVE_SDMC, {PATH_BINARY, 0, (void*) ""}};
        if(R_SUCCEEDED(res = FSUSER_OpenArchive(&sdmcArchive))) {
            if(R_SUCCEEDED(res = util_ensure_dir(&sdmcArchive, "/fbi/")) && R_SUCCEEDED(res = util_ensure_dir(&sdmcArchive, "/fbi/securevalue/"))) {
                char pathBuf[64];
                snprintf(pathBuf, 64, "/fbi/securevalue/%016llX.dat", info->titleId);

                Handle fileHandle = 0;
                if(R_SUCCEEDED(res = FSUSER_OpenFile(&fileHandle, sdmcArchive, fsMakePath(PATH_ASCII, pathBuf), FS_OPEN_WRITE | FS_OPEN_CREATE, 0))) {
                    u32 bytesWritten = 0;
                    res = FSFILE_Write(fileHandle, &bytesWritten, 0, &value, sizeof(u64), FS_WRITE_FLUSH | FS_WRITE_UPDATE_TIME);
                    FSFILE_Close(fileHandle);
                }
            }

            FSUSER_CloseArchive(&sdmcArchive);
        }
    }

    if(R_FAILED(res)) {
        error_display_res(info, ui_draw_title_info, res, "Failed to export secure value.");

        progressbar_destroy(view);
        ui_pop();

        return;
    }

    progressbar_destroy(view);
    ui_pop();

    ui_push(prompt_create("Success", "Secure value exported.", 0xFF000000, false, info, NULL, ui_draw_title_info, action_export_secure_value_end_onresponse));
}

static void action_export_secure_value_onresponse(ui_view* view, void* data, bool response) {
    if(response) {
        prompt_destroy(view);

        ui_push(progressbar_create("Exporting Secure Value", "", data, action_export_secure_value_update, ui_draw_title_info));
    }
}

void action_export_secure_value(title_info* info) {
    ui_push(prompt_create("Confirmation", "Export secure value for the selected title?", 0xFF000000, true, info, NULL, ui_draw_title_info, action_export_secure_value_onresponse));
}