#include <stdio.h>

#include <3ds.h>

#include "action.h"
#include "../../error.h"
#include "../../progressbar.h"
#include "../../prompt.h"

static void action_import_secure_value_end_onresponse(ui_view* view, void* data, bool response) {
    prompt_destroy(view);
}

static void action_import_secure_value_update(ui_view* view, void* data, float* progress, char* progressText) {
    title_info* info = (title_info*) data;

    char pathBuf[64];
    snprintf(pathBuf, 64, "/fbi/securevalue/%016llX.dat", info->titleId);

    Result res = 0;

    FS_Archive sdmcArchive = {ARCHIVE_SDMC, {PATH_BINARY, 0, (void*) ""}};
    Handle fileHandle = 0;
    if(R_SUCCEEDED(res = FSUSER_OpenFileDirectly(&fileHandle, sdmcArchive, fsMakePath(PATH_ASCII, pathBuf), FS_OPEN_READ, 0))) {
        u32 bytesRead = 0;
        u64 value = 0;
        if(R_SUCCEEDED(res = FSFILE_Read(fileHandle, &bytesRead, 0, &value, sizeof(u64)))) {
            res = FSUSER_SetSaveDataSecureValue(value, SECUREVALUE_SLOT_SD, (u32) ((info->titleId >> 8) & 0xFFFFF), (u8) (info->titleId & 0xFF));
        }

        FSFILE_Close(fileHandle);
    }

    if(R_FAILED(res)) {
        error_display_res(info, ui_draw_title_info, res, "Failed to import secure value.");

        progressbar_destroy(view);
        ui_pop();

        return;
    }

    progressbar_destroy(view);
    ui_pop();

    ui_push(prompt_create("Success", "Secure value imported.", 0xFF000000, false, info, NULL, ui_draw_title_info, action_import_secure_value_end_onresponse));
}

static void action_import_secure_value_onresponse(ui_view* view, void* data, bool response) {
    if(response) {
        prompt_destroy(view);

        ui_push(progressbar_create("Importing Secure Value", "", data, action_import_secure_value_update, ui_draw_title_info));
    }
}

void action_import_secure_value(title_info* info) {
    ui_push(prompt_create("Confirmation", "Import secure value for the selected title?", 0xFF000000, true, info, NULL, ui_draw_title_info, action_import_secure_value_onresponse));
}