#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <3ds.h>
#include <3ds/services/fs.h>

#include "action.h"
#include "clipboard.h"
#include "../../error.h"
#include "../../progressbar.h"
#include "../../prompt.h"
#include "../../../util.h"
#include "../task.h"

typedef struct {
    file_info* base;
    u32 processed;
    u32 total;
    char** contents;
} paste_files_data;

static void action_paste_files_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    ui_draw_file_info(view, ((paste_files_data*) data)->base, x1, y1, x2, y2);
}

static void action_paste_files_failure_onresponse(ui_view* view, void* data, bool response) {
    prompt_destroy(view);
}

static void action_paste_files_free_data(paste_files_data* data) {
    util_free_contents(data->contents, data->total);
    free(data);
}

static void action_paste_files_done_onresponse(ui_view* view, void* data, bool response) {
    action_paste_files_free_data((paste_files_data*) data);

    task_refresh_files();

    prompt_destroy(view);
}

static void action_paste_files_update(ui_view* view, void* data, float* progress, char* progressText) {
    paste_files_data* pasteData = (paste_files_data*) data;

    if(hidKeysDown() & KEY_B) {
        progressbar_destroy(view);
        ui_pop();

        ui_push(prompt_create("Failure", "Paste cancelled.", 0xFF000000, false, data, NULL, action_paste_files_draw_top, action_paste_files_done_onresponse));
        return;
    }

    if(pasteData->processed >= pasteData->total) {
        progressbar_destroy(view);
        ui_pop();

        ui_push(prompt_create("Success", "Contents pasted.", 0xFF000000, false, data, NULL, action_paste_files_draw_top, action_paste_files_done_onresponse));
    } else {
        FS_Archive* srcArchive = clipboard_get_archive();
        char* srcPath = pasteData->contents[pasteData->processed];
        FS_Path srcFsPath = fsMakePath(PATH_ASCII, srcPath);

        char baseDstPath[PATH_MAX];
        if(pasteData->base->isDirectory) {
            strncpy(baseDstPath, pasteData->base->path, PATH_MAX);
        } else {
            util_get_parent_path(baseDstPath, pasteData->base->path, PATH_MAX);
        }

        FS_Archive* dstArchive = pasteData->base->archive;
        char dstPath[PATH_MAX];
        util_get_parent_path(dstPath, clipboard_get_path(), PATH_MAX);
        snprintf(dstPath, PATH_MAX, "%s%s", baseDstPath, srcPath + strlen(dstPath));
        FS_Path dstFsPath = fsMakePath(PATH_ASCII, dstPath);

        Result res = 0;

        if(util_is_dir(srcArchive, srcPath)) {
            res = FSUSER_CreateDirectory(*dstArchive, dstFsPath, 0);
        } else {
            Handle srcFileHandle;
            if(R_SUCCEEDED(res = FSUSER_OpenFile(&srcFileHandle, *srcArchive, srcFsPath, FS_OPEN_READ, 0))) {
                u64 size = 0;
                if(R_SUCCEEDED(res = FSFILE_GetSize(srcFileHandle, &size)) && R_SUCCEEDED(res = FSUSER_CreateFile(*dstArchive, dstFsPath, 0, size))) {
                    Handle dstFileHandle;
                    if(R_SUCCEEDED(res = FSUSER_OpenFile(&dstFileHandle, *dstArchive, dstFsPath, FS_OPEN_WRITE, 0))) {
                        u32 bytesRead = 0;
                        u32 offset = 0;
                        u8* buffer = (u8*) calloc(1, 1024 * 512);
                        while(R_SUCCEEDED(FSFILE_Read(srcFileHandle, &bytesRead, offset, buffer, 1024 * 512)) && bytesRead > 0) {
                            u32 bytesWritten = 0;
                            if(R_FAILED(res = FSFILE_Write(dstFileHandle, &bytesWritten, offset, buffer, bytesRead, 0))) {
                                break;
                            }

                            offset += bytesWritten;
                        }

                        free(buffer);

                        FSFILE_Close(dstFileHandle);

                        if(R_SUCCEEDED(res) && dstArchive->id == ARCHIVE_USER_SAVEDATA) {
                            res = FSUSER_ControlArchive(*dstArchive, ARCHIVE_ACTION_COMMIT_SAVE_DATA, NULL, 0, NULL, 0);
                        }
                    }
                }

                FSFILE_Close(srcFileHandle);
            }
        }

        if(R_FAILED(res)) {
            if(pasteData->processed >= pasteData->total - 1) {
                ui_pop();
            }

            if(strlen(srcPath) > 48) {
                error_display_res(pasteData->base, ui_draw_file_info, res, "Failed to paste content.\n%.45s...", srcPath);
            } else {
                error_display_res(pasteData->base, ui_draw_file_info, res, "Failed to paste content.\n%.48s", srcPath);
            }

            if(pasteData->processed >= pasteData->total - 1) {
                action_paste_files_free_data(pasteData);
                progressbar_destroy(view);
                return;
            }
        }

        pasteData->processed++;

        *progress = (float) pasteData->processed / (float) pasteData->total;
        snprintf(progressText, PROGRESS_TEXT_MAX, "%lu / %lu", pasteData->processed, pasteData->total);
    }
}

static void action_paste_files_onresponse(ui_view* view, void* data, bool response) {
    if(response) {
        prompt_destroy(view);

        ui_view* progressView = progressbar_create("Pasting Contents", "Press B to cancel.", data, action_paste_files_update, action_paste_files_draw_top);
        snprintf(progressbar_get_progress_text(progressView), PROGRESS_TEXT_MAX, "0 / %lu", ((paste_files_data*) data)->total);
        ui_push(progressView);
    }
}

void action_paste_contents(file_info* info) {
    if(!clipboard_has_contents()) {
        ui_push(prompt_create("Failure", "Clipboard empty.", 0xFF000000, false, info, NULL, ui_draw_file_info, action_paste_files_failure_onresponse));
        return;
    }

    paste_files_data* data = (paste_files_data*) calloc(1, sizeof(paste_files_data));
    data->base = info;
    data->processed = 0;

    Result res = 0;
    if(R_FAILED(res = util_populate_contents(&data->contents, &data->total, clipboard_get_archive(), clipboard_get_path(), true, true, NULL, NULL))) {
        error_display_res(info, ui_draw_file_info, res, "Failed to retrieve content list.");

        free(data);
        return;
    }

    ui_push(prompt_create("Confirmation", "Paste clipboard contents to the current directory?", 0xFF000000, true, data, NULL, action_paste_files_draw_top, action_paste_files_onresponse));
}