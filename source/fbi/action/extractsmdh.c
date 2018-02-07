#include <stdio.h>
#include <stdlib.h>

#include <3ds.h>

#include "action.h"
#include "../resources.h"
#include "../task/uitask.h"
#include "../../core/core.h"

static void action_extract_smdh_update(ui_view* view, void* data, float* progress, char* text) {
    title_info* info = (title_info*) data;

    Result res = 0;

    static const u32 filePath[5] = {0x00000000, 0x00000000, 0x00000002, 0x6E6F6369, 0x00000000};
    u32 archivePath[4] = {(u32) (info->titleId & 0xFFFFFFFF), (u32) ((info->titleId >> 32) & 0xFFFFFFFF), info->mediaType, 0x00000000};

    Handle fileHandle;
    if(R_SUCCEEDED(res = FSUSER_OpenFileDirectly(&fileHandle, ARCHIVE_SAVEDATA_AND_CONTENT,
                                                 fs_make_path_binary(archivePath, sizeof(archivePath)),
                                                 fs_make_path_binary(filePath, sizeof(filePath)), FS_OPEN_READ, 0))) {
        SMDH* smdh = (SMDH*) calloc(1, sizeof(SMDH));
        if(smdh != NULL) {
            u32 bytesRead = 0;
            if(R_SUCCEEDED(res = FSFILE_Read(fileHandle, &bytesRead, 0, smdh, sizeof(SMDH))) && bytesRead == sizeof(SMDH)) {
                FS_Archive sdmcArchive = 0;
                if(R_SUCCEEDED(res = FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, "")))) {
                    if(R_SUCCEEDED(res = fs_ensure_dir(sdmcArchive, "/fbi/")) && R_SUCCEEDED(res = fs_ensure_dir(sdmcArchive, "/fbi/smdh/"))) {
                        char pathBuf[64];
                        snprintf(pathBuf, 64, "/fbi/smdh/%016llX.smdh", info->titleId);

                        FS_Path* fsPath = fs_make_path_utf8(pathBuf);
                        if(fsPath != NULL) {
                            Handle smdhHandle = 0;
                            if(R_SUCCEEDED(res = FSUSER_OpenFile(&smdhHandle, sdmcArchive, *fsPath, FS_OPEN_WRITE | FS_OPEN_CREATE, 0))) {
                                u32 bytesWritten = 0;
                                res = FSFILE_Write(smdhHandle, &bytesWritten, 0, smdh, sizeof(SMDH), FS_WRITE_FLUSH | FS_WRITE_UPDATE_TIME);
                                FSFILE_Close(smdhHandle);
                            }

                            fs_free_path_utf8(fsPath);
                        } else {
                            res = R_APP_OUT_OF_MEMORY;
                        }
                    }

                    FSUSER_CloseArchive(sdmcArchive);
                }
            }

            free(smdh);
        } else {
            res = R_APP_OUT_OF_MEMORY;
        }

        FSFILE_Close(fileHandle);
    }

    ui_pop();
    info_destroy(view);

    if(R_SUCCEEDED(res)) {
        prompt_display_notify("Success", "SMDH extracted.", COLOR_TEXT, info, task_draw_title_info, NULL);
    } else {
        error_display_res(info, task_draw_title_info, res, "Failed to extract SMDH.");
    }
}

static void action_extract_smdh_onresponse(ui_view* view, void* data, u32 response) {
    if(response == PROMPT_YES) {
        info_display("Extracting SMDH", "", false, data, action_extract_smdh_update, task_draw_title_info);
    }
}

void action_extract_smdh(linked_list* items, list_item* selected) {
    prompt_display_yes_no("Confirmation", "Extract the SMDH of the selected title?", COLOR_TEXT, selected->data, task_draw_title_info, action_extract_smdh_onresponse);
}