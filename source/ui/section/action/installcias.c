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
    bool delete;
    bool* populated;
    Handle currHandle;
    bool installStarted;
    u64 currProcessed;
    u64 currTotal;
    u32 processed;
    u32 total;
    char** contents;

    install_cia_result installResult;
    Handle installCancelEvent;
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
    if(data->currHandle != 0) {
        FSFILE_Close(data->currHandle);
        data->currHandle = 0;
    }

    util_free_contents(data->contents, data->total);
    free(data);
}

static void action_install_cias_done_onresponse(ui_view* view, void* data, bool response) {
    action_install_cias_free_data((install_cias_data*) data);

    prompt_destroy(view);
}

static void action_install_cias_update(ui_view* view, void* data, float* progress, char* progressText) {
    install_cias_data* installData = (install_cias_data*) data;

    if(hidKeysDown() & KEY_B) {
        svcSignalEvent(installData->installCancelEvent);
        while(svcWaitSynchronization(installData->installCancelEvent, 0) == 0) {
            svcSleepThread(1000000);
        }

        installData->installCancelEvent = 0;
    }

    if(!installData->installStarted || installData->installResult.finished) {
        if(installData->currHandle != 0) {
            FSFILE_Close(installData->currHandle);
            installData->currHandle = 0;
        }

        if(installData->installResult.finished) {
            char* path = installData->contents[installData->processed];

            if(installData->installResult.failed) {
                if(installData->installResult.cancelled || installData->processed >= installData->total - 1) {
                    ui_pop();
                    progressbar_destroy(view);
                }

                if(installData->installResult.cancelled) {
                    ui_push(prompt_create("Failure", "Install cancelled.", COLOR_TEXT, false, data, NULL, action_install_cias_draw_top, action_install_cias_done_onresponse));
                } else if(installData->installResult.ioerr) {
                    if(strlen(path) > 48) {
                        error_display_errno(installData->base, ui_draw_file_info, installData->installResult.ioerrno, "Failed to install CIA file.\n%.45s...", path);
                    } else {
                        error_display_errno(installData->base, ui_draw_file_info, installData->installResult.ioerrno, "Failed to install CIA file.\n%.48s", path);
                    }
                } else if(installData->installResult.wrongSystem) {
                    ui_push(prompt_create("Failure", "Attempted to install to wrong system.", COLOR_TEXT, false, data, NULL, action_install_cias_draw_top, action_install_cias_done_onresponse));
                } else {
                    if(strlen(path) > 48) {
                        error_display_res(installData->base, ui_draw_file_info, installData->installResult.result, "Failed to install CIA file.\n%.45s...", path);
                    } else {
                        error_display_res(installData->base, ui_draw_file_info, installData->installResult.result, "Failed to install CIA file.\n%.48s", path);
                    }
                }

                if(installData->installResult.cancelled || installData->processed >= installData->total - 1) {
                    if(!installData->installResult.cancelled && !installData->installResult.wrongSystem) {
                        action_install_cias_free_data(installData);
                    }

                    return;
                }
            } else if(installData->delete) {
                if(R_SUCCEEDED(FSUSER_DeleteFile(*installData->base->archive, fsMakePath(PATH_ASCII, path))) && installData->base->archive->id == ARCHIVE_USER_SAVEDATA) {
                    FSUSER_ControlArchive(*installData->base->archive, ARCHIVE_ACTION_COMMIT_SAVE_DATA, NULL, 0, NULL, 0);
                }

                *installData->populated = false;
            }

            installData->processed++;

            installData->currProcessed = 0;
            installData->currTotal = 0;

            memset(&installData->installResult, 0, sizeof(installData->installResult));
            installData->installCancelEvent = 0;
        }

        if(installData->processed >= installData->total) {
            ui_pop();
            progressbar_destroy(view);

            ui_push(prompt_create("Success", "Install finished.", COLOR_TEXT, false, data, NULL, action_install_cias_draw_top, action_install_cias_done_onresponse));
            return;
        } else {
            char* path = installData->contents[installData->processed];

            Result res = 0;

            if(R_SUCCEEDED(res = FSUSER_OpenFile(&installData->currHandle, *installData->base->archive, fsMakePath(PATH_ASCII, path), FS_OPEN_READ, 0))) {
                u64 size = 0;
                if(R_SUCCEEDED(res = FSFILE_GetSize(installData->currHandle, &size))) {
                    installData->currTotal = size;

                    installData->installCancelEvent = task_install_cia(&installData->installResult, installData->currTotal, installData, action_install_cias_read);
                    if(installData->installCancelEvent != 0) {
                        installData->installStarted = true;
                    } else {
                        ui_pop();
                        progressbar_destroy(view);

                        if(strlen(path) > 48) {
                            error_display_res(installData->base, ui_draw_file_info, res, "Failed to start CIA install.\n%.45s...", path);
                        } else {
                            error_display_res(installData->base, ui_draw_file_info, res, "Failed to start CIA install.\n%.48s", path);
                        }

                        action_install_cias_free_data(installData);

                        return;
                    }
                }
            }

            if(R_FAILED(res)) {
                if(installData->currHandle != 0) {
                    FSFILE_Close(installData->currHandle);
                    installData->currHandle = 0;
                }

                if(installData->processed >= installData->total - 1) {
                    ui_pop();
                    progressbar_destroy(view);
                }

                if(strlen(path) > 48) {
                    error_display_res(installData->base, ui_draw_file_info, res, "Failed to open CIA file.\n%.45s...", path);
                } else {
                    error_display_res(installData->base, ui_draw_file_info, res, "Failed to open CIA file.\n%.48s", path);
                }

                if(installData->processed >= installData->total - 1) {
                    action_install_cias_free_data(installData);

                    return;
                }
            }
        }
    }

    *progress = (float) ((double) installData->currProcessed / (double) installData->currTotal);
    snprintf(progressText, PROGRESS_TEXT_MAX, "%lu / %lu\n%.2f MB / %.2f MB", installData->processed, installData->total, installData->currProcessed / 1024.0 / 1024.0, installData->currTotal / 1024.0 / 1024.0);
}

static void action_install_cias_onresponse(ui_view* view, void* data, bool response) {
    prompt_destroy(view);

    if(response) {
        ui_view* progressView = progressbar_create("Installing CIA(s)", "Press B to cancel.", data, action_install_cias_update, action_install_cias_draw_top);
        snprintf(progressbar_get_progress_text(progressView), PROGRESS_TEXT_MAX, "0 / %lu", ((install_cias_data*) data)->total);
        ui_push(progressView);
    } else {
        free(data);
    }
}

static void action_install_cias_internal(file_info* info, bool* populated, bool delete) {
    install_cias_data* data = (install_cias_data*) calloc(1, sizeof(install_cias_data));
    data->base = info;
    data->delete = delete;
    data->populated = populated;
    data->currHandle = 0;
    data->installStarted = false;
    data->currProcessed = 0;
    data->currTotal = 0;
    data->processed = 0;

    memset(&data->installResult, 0, sizeof(data->installResult));
    data->installCancelEvent = 0;

    Result res = 0;
    if(R_FAILED(res = util_populate_contents(&data->contents, &data->total, info->archive, info->path, false, false, ".cia", util_filter_file_extension))) {
        error_display_res(info, ui_draw_file_info, res, "Failed to retrieve content list.");

        free(data);
        return;
    }

    ui_push(prompt_create("Confirmation", "Install the selected CIA(s)?", COLOR_TEXT, true, data, NULL, action_install_cias_draw_top, action_install_cias_onresponse));
}

void action_install_cias(file_info* info, bool* populated) {
    action_install_cias_internal(info, populated, false);
}

void action_install_cias_delete(file_info* info, bool* populated) {
    action_install_cias_internal(info, populated, true);
}