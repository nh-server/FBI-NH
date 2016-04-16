#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <3ds.h>

#include "action.h"
#include "../task/task.h"
#include "../../error.h"
#include "../../progressbar.h"
#include "../../prompt.h"
#include "../../../screen.h"
#include "../../../util.h"

typedef struct {
    file_info* base;
    u32 processed;
    u32 total;
    char** contents;
} install_tickets_data;

static void action_install_tickets_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    ui_draw_file_info(view, ((install_tickets_data*) data)->base, x1, y1, x2, y2);
}

static void action_install_tickets_free_data(install_tickets_data* data) {
    util_free_contents(data->contents, data->total);
    free(data);
}

static void action_install_tickets_done_onresponse(ui_view* view, void* data, bool response) {
    action_install_tickets_free_data((install_tickets_data*) data);

    prompt_destroy(view);
}

static void action_install_tickets_update(ui_view* view, void* data, float* progress, char* progressText) {
    install_tickets_data* installData = (install_tickets_data*) data;

    if(hidKeysDown() & KEY_B) {
        ui_pop();
        progressbar_destroy(view);

        ui_push(prompt_create("Failed", "Install cancelled.", COLOR_TEXT, false, data, NULL, action_install_tickets_draw_top, action_install_tickets_done_onresponse));
        return;
    }

    if(installData->processed >= installData->total) {
        ui_pop();
        progressbar_destroy(view);

        ui_push(prompt_create("Success", "Install finished.", COLOR_TEXT, false, data, NULL, action_install_tickets_draw_top, action_install_tickets_done_onresponse));
        return;
    } else {
        char* path = installData->contents[installData->processed];

        Result res = 0;

        Handle handle = 0;
        if(R_SUCCEEDED(res = FSUSER_OpenFile(&handle, *installData->base->archive, fsMakePath(PATH_ASCII, path), FS_OPEN_READ, 0))) {
            u64 size = 0;
            if(R_SUCCEEDED(res = FSFILE_GetSize(handle, &size))) {
                u8* buffer = (u8*) calloc((size_t) size, 1);
                if(buffer != NULL) {
                    u32 bytesRead = 0;
                    if(R_SUCCEEDED(res = FSFILE_Read(handle, &bytesRead, 0, buffer, (u32) size)) && bytesRead == size) {
                        Handle ticketHandle;
                        if(R_SUCCEEDED(res = AM_InstallTicketBegin(&ticketHandle))) {
                            u32 bytesWritten = 0;
                            if(R_FAILED(res = FSFILE_Write(ticketHandle, &bytesWritten, 0, buffer, (u32) size, 0)) || bytesWritten != size || R_FAILED(res = AM_InstallTicketFinalize(ticketHandle))) {
                                AM_InstallTicketAbort(ticketHandle);
                            }
                        }
                    }

                    free(buffer);
                }
            }

            FSFILE_Close(handle);
        }

        installData->processed++;

        if(R_FAILED(res)) {
            if(installData->processed >= installData->total - 1) {
                ui_pop();
            }

            if(strlen(path) > 48) {
                error_display_res(installData->base, ui_draw_file_info, res, "Failed to install ticket.\n%.45s...", path);
            } else {
                error_display_res(installData->base, ui_draw_file_info, res, "Failed to install ticket.\n%.48s", path);
            }

            if(installData->processed >= installData->total - 1) {
                action_install_tickets_free_data(installData);
                progressbar_destroy(view);

                return;
            }
        }
    }

    *progress = (float) ((double) installData->processed / (double) installData->total);
    snprintf(progressText, PROGRESS_TEXT_MAX, "%lu / %lu", installData->processed, installData->total);
}

static void action_install_tickets_onresponse(ui_view* view, void* data, bool response) {
    prompt_destroy(view);

    if(response) {
        ui_view* progressView = progressbar_create("Installing tickets(s)", "Press B to cancel.", data, action_install_tickets_update, action_install_tickets_draw_top);
        snprintf(progressbar_get_progress_text(progressView), PROGRESS_TEXT_MAX, "0 / %lu", ((install_tickets_data*) data)->total);
        ui_push(progressView);
    } else {
        free(data);
    }
}

void action_install_tickets(file_info* info, bool* populated) {
    install_tickets_data* data = (install_tickets_data*) calloc(1, sizeof(install_tickets_data));
    data->base = info;
    data->processed = 0;

    Result res = 0;
    if(R_FAILED(res = util_populate_contents(&data->contents, &data->total, info->archive, info->path, false, false, ".tik", util_filter_file_extension))) {
        error_display_res(info, ui_draw_file_info, res, "Failed to retrieve content list.");

        free(data);
        return;
    }

    ui_push(prompt_create("Confirmation", "Install the selected ticket(s)?", COLOR_TEXT, true, data, NULL, action_install_tickets_draw_top, action_install_tickets_onresponse));
}