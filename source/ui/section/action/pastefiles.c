#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <3ds.h>

#include "action.h"
#include "clipboard.h"
#include "../../error.h"
#include "../../progressbar.h"
#include "../../prompt.h"
#include "../../../screen.h"
#include "../../../util.h"

#define PASTE_BUFFER_SIZE (1024 * 512)

typedef struct {
    file_info* base;
    bool* populated;
    bool started;
    u8* buffer;
    Handle currSrc;
    Handle currDst;
    u64 currProcessed;
    u64 currTotal;
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
    free(data->buffer);
    free(data);
}

static void action_paste_files_done_onresponse(ui_view* view, void* data, bool response) {
    action_paste_files_free_data((paste_files_data*) data);

    prompt_destroy(view);
}

static void action_paste_files_update(ui_view* view, void* data, float* progress, char* progressText) {
    paste_files_data* pasteData = (paste_files_data*) data;

    if(hidKeysDown() & KEY_B) {
        if(pasteData->currSrc != 0) {
            FSFILE_Close(pasteData->currSrc);
            pasteData->currSrc = 0;
        }

        if(pasteData->currDst != 0) {
            FSFILE_Close(pasteData->currDst);
            pasteData->currDst = 0;
        }

        progressbar_destroy(view);
        ui_pop();

        ui_push(prompt_create("Failure", "Paste cancelled.", COLOR_TEXT, false, data, NULL, action_paste_files_draw_top, action_paste_files_done_onresponse));
        return;
    }

    if(!pasteData->started || pasteData->currProcessed >= pasteData->currTotal) {
        if(pasteData->started) {
            if(pasteData->currSrc != 0) {
                FSFILE_Close(pasteData->currSrc);
                pasteData->currSrc = 0;
            }

            if(pasteData->currDst != 0) {
                FSFILE_Close(pasteData->currDst);
                pasteData->currDst = 0;
            }

            if(pasteData->base->archive->id == ARCHIVE_USER_SAVEDATA) {
                FSUSER_ControlArchive(*pasteData->base->archive, ARCHIVE_ACTION_COMMIT_SAVE_DATA, NULL, 0, NULL, 0);
            }

            pasteData->currProcessed = 0;
            pasteData->currTotal = 0;

            pasteData->processed++;
        }

        if(pasteData->processed >= pasteData->total) {
            ui_pop();
            progressbar_destroy(view);

            ui_push(prompt_create("Success", "Contents pasted.", COLOR_TEXT, false, data, NULL, action_paste_files_draw_top, action_paste_files_done_onresponse));
            return;
        } else {
            FS_Archive* srcArchive = clipboard_get_archive();
            char* srcPath = pasteData->contents[pasteData->processed];

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

            Result res = 0;

            if(util_is_dir(srcArchive, srcPath)) {
                res = FSUSER_CreateDirectory(*dstArchive, fsMakePath(PATH_ASCII, dstPath), 0);
            } else {
                if(R_SUCCEEDED(res = FSUSER_OpenFile(&pasteData->currSrc, *srcArchive, fsMakePath(PATH_ASCII, srcPath), FS_OPEN_READ, 0)) && R_SUCCEEDED(res = FSFILE_GetSize(pasteData->currSrc, &pasteData->currTotal))) {
                    res = FSUSER_OpenFile(&pasteData->currDst, *dstArchive, fsMakePath(PATH_ASCII, dstPath), FS_OPEN_WRITE | FS_OPEN_CREATE, 0);
                }
            }

            if(R_FAILED(res)) {
                if(pasteData->currSrc != 0) {
                    FSFILE_Close(pasteData->currSrc);
                    pasteData->currSrc = 0;
                }

                if(pasteData->currDst != 0) {
                    FSFILE_Close(pasteData->currDst);
                    pasteData->currDst = 0;
                }

                if(pasteData->processed >= pasteData->total - 1) {
                    ui_pop();
                    progressbar_destroy(view);
                }

                if(strlen(srcPath) > 48) {
                    error_display_res(pasteData->base, ui_draw_file_info, res, "Failed to paste content.\n%.45s...", srcPath);
                } else {
                    error_display_res(pasteData->base, ui_draw_file_info, res, "Failed to paste content.\n%.48s", srcPath);
                }

                if(pasteData->processed >= pasteData->total - 1) {
                    action_paste_files_free_data(pasteData);

                    return;
                }
            } else {
                *pasteData->populated = false;

                pasteData->started = true;
            }
        }
    } else if(pasteData->currSrc != 0 && pasteData->currDst != 0) {
        Result res = 0;

        u32 size = PASTE_BUFFER_SIZE;
        if((u64) size > pasteData->currTotal - pasteData->currProcessed) {
            size = (u32) (pasteData->currTotal - pasteData->currProcessed);
        }

        u32 bytesRead = 0;
        if(R_SUCCEEDED(res = FSFILE_Read(pasteData->currSrc, &bytesRead, pasteData->currProcessed, pasteData->buffer, size)) && bytesRead > 0) {
            u32 bytesWritten = 0;
            if(R_SUCCEEDED(res = FSFILE_Write(pasteData->currDst, &bytesWritten, pasteData->currProcessed, pasteData->buffer, bytesRead, 0))) {
                pasteData->currProcessed += bytesWritten;
            }
        }

        if(R_FAILED(res)) {
            if(pasteData->currSrc != 0) {
                FSFILE_Close(pasteData->currSrc);
                pasteData->currSrc = 0;
            }

            if(pasteData->currDst != 0) {
                FSFILE_Close(pasteData->currDst);
                pasteData->currDst = 0;
            }

            if(pasteData->processed >= pasteData->total - 1) {
                ui_pop();
                progressbar_destroy(view);
            }

            char* srcPath = pasteData->contents[pasteData->processed];
            if(strlen(srcPath) > 48) {
                error_display_res(pasteData->base, ui_draw_file_info, res, "Failed to paste content.\n%.45s...", srcPath);
            } else {
                error_display_res(pasteData->base, ui_draw_file_info, res, "Failed to paste content.\n%.48s", srcPath);
            }

            pasteData->currProcessed = pasteData->currTotal;

            if(pasteData->processed >= pasteData->total - 1) {
                action_paste_files_free_data(pasteData);

                return;
            }
        }
    }

    *progress = pasteData->currTotal != 0 ? (float) ((double) pasteData->currProcessed / (double) pasteData->currTotal) : 1;
    snprintf(progressText, PROGRESS_TEXT_MAX, "%lu / %lu\n%.2f MB / %.2f MB", pasteData->processed, pasteData->total, pasteData->currProcessed / 1024.0 / 1024.0, pasteData->currTotal / 1024.0 / 1024.0);
}

static void action_paste_files_onresponse(ui_view* view, void* data, bool response) {
    prompt_destroy(view);

    if(response) {
        ui_view* progressView = progressbar_create("Pasting Contents", "Press B to cancel.", data, action_paste_files_update, action_paste_files_draw_top);
        snprintf(progressbar_get_progress_text(progressView), PROGRESS_TEXT_MAX, "0 / %lu", ((paste_files_data*) data)->total);
        ui_push(progressView);
    } else {
        free(data);
    }
}

void action_paste_contents(file_info* info, bool* populated) {
    if(!clipboard_has_contents()) {
        ui_push(prompt_create("Failure", "Clipboard empty.", COLOR_TEXT, false, info, NULL, ui_draw_file_info, action_paste_files_failure_onresponse));
        return;
    }

    paste_files_data* data = (paste_files_data*) calloc(1, sizeof(paste_files_data));
    data->base = info;
    data->populated = populated;
    data->started = false;
    data->buffer = (u8*) calloc(1, PASTE_BUFFER_SIZE);
    data->currSrc = 0;
    data->currDst = 0;
    data->currProcessed = 0;
    data->currTotal = 0;
    data->processed = 0;

    Result res = 0;
    if(R_FAILED(res = util_populate_contents(&data->contents, &data->total, clipboard_get_archive(), clipboard_get_path(), true, true, NULL, NULL))) {
        error_display_res(info, ui_draw_file_info, res, "Failed to retrieve content list.");

        free(data);
        return;
    }

    ui_push(prompt_create("Confirmation", "Paste clipboard contents to the current directory?", COLOR_TEXT, true, data, NULL, action_paste_files_draw_top, action_paste_files_onresponse));
}