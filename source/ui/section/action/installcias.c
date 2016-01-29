#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <3ds.h>

#include "action.h"
#include "../../error.h"
#include "../../progressbar.h"
#include "../../prompt.h"
#include "../../../util.h"

typedef struct {
    file_info* base;
    FS_MediaType dest;
    Handle currHandle;
    bool installStarted;
    u64 currProcessed;
    u64 currTotal;
    u32 processed;
    u32 total;
    char** contents;
} install_cias_data;

static Result action_install_cias_read(void* data, u32* bytesRead, void* buffer, u32 size) {
    install_cias_data* installData = (install_cias_data*) data;

    u32 read = 0;
    Result res = FSFILE_Read(installData->currHandle, &read, installData->currProcessed, buffer, size);

    if(R_SUCCEEDED(res)) {
        installData->currProcessed += read;
        if(bytesRead != NULL) {
            *bytesRead = read;
        }
    }

    return res;
}

static void action_install_cias_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    ui_draw_file_info(view, ((install_cias_data*) data)->base, x1, y1, x2, y2);
}

static void action_install_cias_free_data(install_cias_data* data) {
    util_free_contents(data->contents, data->total);
    free(data);
}

static void action_install_cias_done_onresponse(ui_view* view, void* data, bool response) {
    action_install_cias_free_data((install_cias_data*) data);

    task_refresh_titles();

    prompt_destroy(view);
}

static void action_install_cias_update(ui_view* view, void* data, float* progress, char* progressText) {
    install_cias_data* installData = (install_cias_data*) data;

    bool cancelled = false;
    if(hidKeysDown() & KEY_B) {
        task_cancel_cia_install();

        while(task_is_cia_installing()) {
            svcSleepThread(1000000);
        }

        cancelled = true;
    }

    if(!task_is_cia_installing()) {
        char* path = installData->contents[installData->processed];

        if(installData->installStarted || cancelled) {
            if(installData->currHandle != 0) {
                FSFILE_Close(installData->currHandle);
                installData->currHandle = 0;
            }

            Result res = task_get_cia_install_result();
            if(R_FAILED(res)) {
                if(installData->processed >= installData->total - 1) {
                    ui_pop();
                }

                if(res == CIA_INSTALL_RESULT_CANCELLED) {
                    ui_push(prompt_create("Failure", "Install cancelled.", 0xFF000000, false, data, NULL, action_install_cias_draw_top, action_install_cias_done_onresponse));
                } else if(res == CIA_INSTALL_RESULT_ERRNO) {
                    if(strlen(path) > 48) {
                        error_display_errno(installData->base, ui_draw_file_info, task_get_cia_install_errno(), "Failed to install CIA file.\n%.45s...", path);
                    } else {
                        error_display_errno(installData->base, ui_draw_file_info, task_get_cia_install_errno(), "Failed to install CIA file.\n%.48s", path);
                    }
                } else if(res == CIA_INSTALL_RESULT_WRONG_SYSTEM) {
                    ui_push(prompt_create("Failure", "Attempted to install to wrong system.", 0xFF000000, false, data, NULL, action_install_cias_draw_top, action_install_cias_done_onresponse));
                } else {
                    if(strlen(path) > 48) {
                        error_display_res(installData->base, ui_draw_file_info, res, "Failed to install CIA file.\n%.45s...", path);
                    } else {
                        error_display_res(installData->base, ui_draw_file_info, res, "Failed to install CIA file.\n%.48s", path);
                    }
                }

                if(installData->processed >= installData->total - 1) {
                    if(res != CIA_INSTALL_RESULT_CANCELLED && res != CIA_INSTALL_RESULT_WRONG_SYSTEM) {
                        action_install_cias_free_data(installData);
                    }

                    progressbar_destroy(view);
                    return;
                }
            }

            installData->processed++;
        }

        installData->installStarted = true;

        if(installData->processed >= installData->total) {
            progressbar_destroy(view);
            ui_pop();

            ui_push(prompt_create("Success", "Install finished.", 0xFF000000, false, data, NULL, action_install_cias_draw_top, action_install_cias_done_onresponse));
            return;
        } else {
            FS_Archive* archive = installData->base->archive;
            FS_Path fsPath = fsMakePath(PATH_ASCII, path);

            Result res = 0;

            if(R_SUCCEEDED(res = FSUSER_OpenFile(&installData->currHandle, *archive, fsPath, FS_OPEN_READ, 0))) {
                u64 size = 0;
                if(R_SUCCEEDED(res = FSFILE_GetSize(installData->currHandle, &size))) {
                    installData->currTotal = size;

                    task_request_cia_install(installData->dest, installData->currTotal, installData, action_install_cias_read);
                }
            }

            if(R_FAILED(res)) {
                if(installData->currHandle != 0) {
                    FSFILE_Close(installData->currHandle);
                    installData->currHandle = 0;
                }

                if(installData->processed >= installData->total - 1) {
                    ui_pop();
                }

                if(strlen(path) > 48) {
                    error_display_res(installData->base, ui_draw_file_info, res, "Failed to open CIA file.\n%.45s...", path);
                } else {
                    error_display_res(installData->base, ui_draw_file_info, res, "Failed to open CIA file.\n%.48s", path);
                }

                if(installData->processed >= installData->total - 1) {
                    action_install_cias_free_data(installData);
                    progressbar_destroy(view);
                    return;
                }
            }
        }
    }

    *progress = (float) ((double) installData->currProcessed / (double) installData->currTotal);
    snprintf(progressText, PROGRESS_TEXT_MAX, "%lu / %lu\n%.2f MB / %.2f MB", installData->processed, installData->total, installData->currProcessed / 1024.0 / 1024.0, installData->currTotal / 1024.0 / 1024.0);
}

static void action_install_cias_onresponse(ui_view* view, void* data, bool response) {
    if(response) {
        prompt_destroy(view);

        ui_view* progressView = progressbar_create("Installing CIA(s)", "Press B to cancel.", data, action_install_cias_update, action_install_cias_draw_top);
        snprintf(progressbar_get_progress_text(progressView), PROGRESS_TEXT_MAX, "0 / %lu", ((install_cias_data*) data)->total);
        ui_push(progressView);
    }
}

static void action_install_cias(file_info* info, FS_MediaType mediaType) {
    install_cias_data* data = (install_cias_data*) calloc(1, sizeof(install_cias_data));
    data->base = info;
    data->dest = mediaType;
    data->installStarted = false;
    data->currProcessed = 0;
    data->currTotal = 0;
    data->processed = 0;

    Result res = 0;
    if(R_FAILED(res = util_populate_contents(&data->contents, &data->total, info->archive, info->path, false, false, ".cia", util_filter_file_extension))) {
        error_display_res(info, ui_draw_file_info, res, "Failed to retrieve content list.");

        free(data);
        return;
    }

    ui_push(prompt_create("Confirmation", "Install the selected CIA(s)?", 0xFF000000, true, data, NULL, action_install_cias_draw_top, action_install_cias_onresponse));
}

void action_install_cias_sd(file_info* info) {
    action_install_cias(info, MEDIATYPE_SD);
}

void action_install_cias_nand(file_info* info) {
    action_install_cias(info, MEDIATYPE_NAND);
}