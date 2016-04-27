#include <stdio.h>
#include <stdlib.h>

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
#include "../../../core/util.h"

static void action_extract_smdh_update(ui_view* view, void* data, float* progress, char* text) {
    title_info* info = (title_info*) data;

    Result res = 0;

    static const u32 filePathData[] = {0x00000000, 0x00000000, 0x00000002, 0x6E6F6369, 0x00000000};
    static const FS_Path filePath = (FS_Path) {PATH_BINARY, 0x14, (u8*) filePathData};
    u32 archivePath[] = {(u32) (info->titleId & 0xFFFFFFFF), (u32) ((info->titleId >> 32) & 0xFFFFFFFF), info->mediaType, 0x00000000};
    FS_Archive archive = {ARCHIVE_SAVEDATA_AND_CONTENT, (FS_Path) {PATH_BINARY, 0x10, (u8*) archivePath}};

    Handle fileHandle;
    if(R_SUCCEEDED(res = FSUSER_OpenFileDirectly(&fileHandle, archive, filePath, FS_OPEN_READ, 0))) {
        SMDH smdh;

        u32 bytesRead = 0;
        if(R_SUCCEEDED(res = FSFILE_Read(fileHandle, &bytesRead, 0, &smdh, sizeof(SMDH))) && bytesRead == sizeof(SMDH)) {
            FS_Archive sdmcArchive = {ARCHIVE_SDMC, {PATH_BINARY, 0, (void*) ""}};
            if(R_SUCCEEDED(res = FSUSER_OpenArchive(&sdmcArchive))) {
                if(R_SUCCEEDED(res = util_ensure_dir(&sdmcArchive, "/fbi/")) && R_SUCCEEDED(res = util_ensure_dir(&sdmcArchive, "/fbi/smdh/"))) {
                    char pathBuf[64];
                    snprintf(pathBuf, 64, "/fbi/smdh/%016llX.smdh", info->titleId);

                    FS_Path* fsPath = util_make_path_utf8(pathBuf);
                    if(fsPath != NULL) {
                        Handle smdhHandle = 0;
                        if(R_SUCCEEDED(res = FSUSER_OpenFile(&smdhHandle, sdmcArchive, *fsPath, FS_OPEN_WRITE | FS_OPEN_CREATE, 0))) {
                            u32 bytesWritten = 0;
                            res = FSFILE_Write(smdhHandle, &bytesWritten, 0, &smdh, sizeof(SMDH), FS_WRITE_FLUSH | FS_WRITE_UPDATE_TIME);
                            FSFILE_Close(smdhHandle);
                        }

                        util_free_path_utf8(fsPath);
                    } else {
                        res = R_FBI_OUT_OF_MEMORY;
                    }
                }

                FSUSER_CloseArchive(&sdmcArchive);
            }
        }

        FSFILE_Close(fileHandle);
    }

    ui_pop();
    info_destroy(view);

    if(R_SUCCEEDED(res)) {
        prompt_display("Success", "SMDH extracted.", COLOR_TEXT, false, info, NULL, ui_draw_title_info, NULL);
    } else {
        error_display_res(NULL, info, ui_draw_title_info, res, "Failed to extract SMDH.");
    }
}

static void action_extract_smdh_onresponse(ui_view* view, void* data, bool response) {
    if(response) {
        info_display("Extracting SMDH", "", false, data, action_extract_smdh_update, ui_draw_title_info);
    }
}

void action_extract_smdh(linked_list* items, list_item* selected) {
    prompt_display("Confirmation", "Extract the SMDH of the selected title?", COLOR_TEXT, true, selected->data, NULL, ui_draw_title_info, action_extract_smdh_onresponse);
}