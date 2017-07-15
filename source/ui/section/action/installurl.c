#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <3ds.h>

#include "../action/action.h"
#include "../task/task.h"
#include "../../error.h"
#include "../../info.h"
#include "../../prompt.h"
#include "../../ui.h"
#include "../../../core/screen.h"
#include "../../../core/util.h"

typedef struct {
    char urls[INSTALL_URLS_MAX][INSTALL_URL_MAX];

    void* userData;
    void (*finished)(void* data);
    void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2, u32 index);

    bool cdn;
    bool selectCdnVersion;
    bool cdnDecided;

    u32 responseCode;
    bool ticket;
    u64 currTitleId;
    volatile bool n3dsContinue;
    ticket_info ticketInfo;

    data_op_data installInfo;
} install_url_data;

static void action_install_url_free_data(install_url_data* data) {
    if(data->finished != NULL) {
        data->finished(data->userData);
    }

    free(data);
}

#define CDN_PROMPT_DEFAULT_VERSION 0
#define CDN_PROMPT_SELECT_VERSION 1
#define CDN_PROMPT_NO 2

static void action_install_url_cdn_check_onresponse(ui_view* view, void* data, u32 response) {
    install_url_data* installData = (install_url_data*) data;

    installData->cdn = response != CDN_PROMPT_NO;
    installData->selectCdnVersion = response == CDN_PROMPT_SELECT_VERSION;
    installData->cdnDecided = true;
}

static void action_install_url_n3ds_onresponse(ui_view* view, void* data, u32 response) {
    ((install_url_data*) data)->n3dsContinue = response == PROMPT_YES;
}

static void action_install_url_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    install_url_data* installData = (install_url_data*) data;

    if(installData->drawTop != NULL) {
        installData->drawTop(view, installData->userData, x1, y1, x2, y2, installData->installInfo.processed);
    } else if(installData->installInfo.processed == installData->installInfo.total) {
        float urlY = y1 + 5;
        u32 index = 0;
        while(urlY < y2 && index < installData->installInfo.total) {
            float urlWidth = 0;
            float urlHeight = 0;
            screen_get_string_size_wrap(&urlWidth, &urlHeight, installData->urls[index], 0.5f, 0.5f, x2 - x1 - 10);

            float urlX = (x2 - x1 - urlWidth) / 2;
            screen_draw_string_wrap(installData->urls[index], urlX, urlY, 0.5f, 0.5f, COLOR_TEXT, false, urlX + urlWidth + 1);

            urlY += urlHeight;
            index++;
        }
    } else {
        float urlWidth = 0;
        float urlHeight = 0;
        screen_get_string_size_wrap(&urlWidth, &urlHeight, installData->urls[installData->installInfo.processed], 0.5f, 0.5f, x2 - x1 - 10);

        float urlX = (x2 - x1 - urlWidth) / 2;
        float urlY = (y2 - y1 - urlHeight) / 2;
        screen_draw_string_wrap(installData->urls[installData->installInfo.processed], urlX, urlY, 0.5f, 0.5f, COLOR_TEXT, false, urlX + urlWidth + 1);
    }
}

static Result action_install_url_is_src_directory(void* data, u32 index, bool* isDirectory) {
    *isDirectory = false;
    return 0;
}

static Result action_install_url_make_dst_directory(void* data, u32 index) {
    return 0;
}

static Result action_install_url_open_src(void* data, u32 index, u32* handle) {
    install_url_data* installData = (install_url_data*) data;

    Result res = 0;

    httpcContext* context = (httpcContext*) calloc(1, sizeof(httpcContext));
    if(context != NULL) {
        if(R_SUCCEEDED(res = util_http_open(context, &installData->responseCode, installData->urls[index], true))) {
            *handle = (u32) context;
        } else {
            free(context);
        }
    } else {
        res = R_FBI_OUT_OF_MEMORY;
    }

    return res;
}

static Result action_install_url_close_src(void* data, u32 index, bool succeeded, u32 handle) {
    return util_http_close((httpcContext*) handle);
}

static Result action_install_url_get_src_size(void* data, u32 handle, u64* size) {
    u32 downloadSize = 0;
    Result res = util_http_get_size((httpcContext*) handle, &downloadSize);

    *size = downloadSize;
    return res;
}

static Result action_install_url_read_src(void* data, u32 handle, u32* bytesRead, void* buffer, u64 offset, u32 size) {
    return util_http_read((httpcContext*) handle, bytesRead, buffer, size);
}

static Result action_install_url_open_dst(void* data, u32 index, void* initialReadBlock, u64 size, u32* handle) {
    install_url_data* installData = (install_url_data*) data;

    Result res = 0;

    installData->responseCode = 0;
    installData->ticket = false;
    installData->currTitleId = 0;
    installData->n3dsContinue = false;
    memset(&installData->ticketInfo, 0, sizeof(installData->ticketInfo));

    if(*(u16*) initialReadBlock == 0x0100) {
        if(!installData->cdnDecided) {
            static const char* options[3] = {"Default\nVersion", "Select\nVersion", "No"};
            static u32 optionButtons[3] = {KEY_A, KEY_X, KEY_B};
            ui_view* view = prompt_display_multi_choice("Optional", "Install ticket titles from CDN?", COLOR_TEXT, options, optionButtons, 3, data, action_install_url_draw_top, action_install_url_cdn_check_onresponse);
            if(view != NULL) {
                svcWaitSynchronization(view->active, U64_MAX);
            }
        }

        installData->ticket = true;
        installData->ticketInfo.titleId = util_get_ticket_title_id((u8*) initialReadBlock);
        installData->ticketInfo.inUse = false;

        AM_DeleteTicket(installData->ticketInfo.titleId);
        res = AM_InstallTicketBegin(handle);
    } else if(*(u16*) initialReadBlock == 0x2020) {
        u64 titleId = util_get_cia_title_id((u8*) initialReadBlock);

        FS_MediaType dest = util_get_title_destination(titleId);

        bool n3ds = false;
        if(R_SUCCEEDED(APT_CheckNew3DS(&n3ds)) && !n3ds && ((titleId >> 28) & 0xF) == 2) {
            ui_view* view = prompt_display_yes_no("Confirmation", "Title is intended for New 3DS systems.\nContinue?", COLOR_TEXT, data, action_install_url_draw_top, action_install_url_n3ds_onresponse);
            if(view != NULL) {
                svcWaitSynchronization(view->active, U64_MAX);
            }

            if(!installData->n3dsContinue) {
                return R_FBI_WRONG_SYSTEM;
            }
        }

        // Deleting FBI before it reinstalls itself causes issues.
        u64 currTitleId = 0;
        FS_MediaType currMediaType = MEDIATYPE_NAND;

        if(envIsHomebrew() || R_FAILED(APT_GetAppletInfo((NS_APPID) envGetAptAppId(), &currTitleId, (u8*) &currMediaType, NULL, NULL, NULL)) || titleId != currTitleId || dest != currMediaType) {
            AM_DeleteTitle(dest, titleId);
            AM_DeleteTicket(titleId);

            if(dest == MEDIATYPE_SD) {
                AM_QueryAvailableExternalTitleDatabase(NULL);
            }
        }

        if(R_SUCCEEDED(res = AM_StartCiaInstall(dest, handle))) {
            installData->currTitleId = titleId;
        }
    } else {
        res = R_FBI_BAD_DATA;
    }

    return res;
}

static Result action_install_url_close_dst(void* data, u32 index, bool succeeded, u32 handle) {
    install_url_data* installData = (install_url_data*) data;

    if(succeeded) {
        Result res = 0;

        if(installData->ticket) {
            res = AM_InstallTicketFinish(handle);

            if(R_SUCCEEDED(res) && installData->cdn) {
                volatile bool done = false;
                action_install_cdn_noprompt(&done, &installData->ticketInfo, false, installData->selectCdnVersion);

                while(!done) {
                    svcSleepThread(100000000);
                }
            }
        } else {
            if(R_SUCCEEDED(res = AM_FinishCiaInstall(handle))) {
                util_import_seed(NULL, installData->currTitleId);

                if(installData->currTitleId == 0x0004013800000002 || installData->currTitleId == 0x0004013820000002) {
                    res = AM_InstallFirm(installData->currTitleId);
                }
            }
        }

        return res;
    } else {
        if(installData->ticket) {
            return AM_InstallTicketAbort(handle);
        } else {
            return AM_CancelCIAInstall(handle);
        }
    }
}

static Result action_install_url_write_dst(void* data, u32 handle, u32* bytesWritten, void* buffer, u64 offset, u32 size) {
    return FSFILE_Write(handle, bytesWritten, offset, buffer, size, 0);
}

static Result action_install_url_suspend_copy(void* data, u32 index, u32* srcHandle, u32* dstHandle) {
    return 0;
}

static Result action_install_url_restore_copy(void* data, u32 index, u32* srcHandle, u32* dstHandle) {
    return 0;
}

static Result action_install_url_suspend(void* data, u32 index) {
    return 0;
}

static Result action_install_url_restore(void* data, u32 index) {
    return 0;
}

static bool action_install_url_error(void* data, u32 index, Result res) {
    install_url_data* installData = (install_url_data*) data;

    if(res == R_FBI_CANCELLED) {
        prompt_display_notify("Failure", "Install cancelled.", COLOR_TEXT, NULL, NULL, NULL);
        return false;
    } else if(res != R_FBI_WRONG_SYSTEM) {
        char* url = installData->urls[index];

        ui_view* view = NULL;

        if(res == R_FBI_HTTP_RESPONSE_CODE) {
            if(strlen(url) > 38) {
                view = error_display(data, action_install_url_draw_top, "Failed to install from URL.\n%.35s...\nHTTP server returned response code %d", url, installData->responseCode);
            } else {
                view = error_display(data, action_install_url_draw_top, "Failed to install from URL.\n%.38s\nHTTP server returned response code %d", url, installData->responseCode);
            }
        } else {
            if(strlen(url) > 38) {
                view = error_display_res(data, action_install_url_draw_top, res, "Failed to install from URL.\n%.35s...", url);
            } else {
                view = error_display_res(data, action_install_url_draw_top, res, "Failed to install from URL.\n%.38s", url);
            }
        }

        if(view != NULL) {
            svcWaitSynchronization(view->active, U64_MAX);
        }
    }

    return index < installData->installInfo.total - 1;
}

static void action_install_url_install_update(ui_view* view, void* data, float* progress, char* text) {
    install_url_data* installData = (install_url_data*) data;

    if(installData->installInfo.finished) {
        ui_pop();
        info_destroy(view);

        if(R_SUCCEEDED(installData->installInfo.result)) {
            prompt_display_notify("Success", "Install finished.", COLOR_TEXT, NULL, NULL, NULL);
        }

        action_install_url_free_data(installData);

        return;
    }

    if(hidKeysDown() & KEY_B) {
        svcSignalEvent(installData->installInfo.cancelEvent);
    }

    *progress = installData->installInfo.currTotal != 0 ? (float) ((double) installData->installInfo.currProcessed / (double) installData->installInfo.currTotal) : 0;
    snprintf(text, PROGRESS_TEXT_MAX, "%lu / %lu\n%.2f %s / %.2f %s\n%.2f %s/s, ETA %s", installData->installInfo.processed, installData->installInfo.total, util_get_display_size(installData->installInfo.currProcessed), util_get_display_size_units(installData->installInfo.currProcessed), util_get_display_size(installData->installInfo.currTotal), util_get_display_size_units(installData->installInfo.currTotal), util_get_display_size(installData->installInfo.copyBytesPerSecond), util_get_display_size_units(installData->installInfo.copyBytesPerSecond), util_get_display_eta(installData->installInfo.estimatedRemainingSeconds));
}

static void action_install_url_confirm_onresponse(ui_view* view, void* data, u32 response) {
    install_url_data* installData = (install_url_data*) data;

    if(response == PROMPT_YES) {
        Result res = task_data_op(&installData->installInfo);
        if(R_SUCCEEDED(res)) {
            info_display("Installing From URL(s)", "Press B to cancel.", true, data, action_install_url_install_update, action_install_url_draw_top);
        } else {
            error_display_res(NULL, NULL, res, "Failed to initiate installation.");

            action_install_url_free_data(installData);
        }
    } else {
        action_install_url_free_data(installData);
    }
}

void action_install_url(const char* confirmMessage, const char* urls, void* userData, void (*finished)(void* data),
                                                                                      void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2, u32 index)) {
    install_url_data* data = (install_url_data*) calloc(1, sizeof(install_url_data));
    if(data == NULL) {
        error_display(NULL, NULL, "Failed to allocate URL install data.");

        return;
    }

    data->installInfo.total = 0;

    size_t payloadLen = strlen(urls);
    if(payloadLen > 0) {
        const char* currStart = urls;
        while(data->installInfo.total < INSTALL_URLS_MAX && currStart - urls < payloadLen) {
            const char* currEnd = strchr(currStart, '\n');
            if(currEnd == NULL) {
                currEnd = urls + payloadLen;
            }

            u32 len = currEnd - currStart;

            if((len < 7 || strncmp(currStart, "http://", 7) != 0) && (len < 8 || strncmp(currStart, "https://", 8) != 0)) {
                if(len > INSTALL_URL_MAX - 7) {
                    len = INSTALL_URL_MAX - 7;
                }

                strncpy(data->urls[data->installInfo.total], "http://", 7);
                strncpy(&data->urls[data->installInfo.total][7], currStart, len);
            } else {
                if(len > INSTALL_URL_MAX) {
                    len = INSTALL_URL_MAX;
                }

                strncpy(data->urls[data->installInfo.total], currStart, len);
            }

            data->installInfo.total++;
            currStart = currEnd + 1;
        }
    }

    data->userData = userData;
    data->finished = finished;
    data->drawTop = drawTop;

    data->cdn = false;
    data->selectCdnVersion = false;
    data->cdnDecided = false;

    data->responseCode = 0;
    data->ticket = false;
    data->currTitleId = 0;
    data->n3dsContinue = false;
    memset(&data->ticketInfo, 0, sizeof(data->ticketInfo));

    data->installInfo.data = data;

    data->installInfo.op = DATAOP_COPY;

    data->installInfo.copyBufferSize = 128 * 1024;
    data->installInfo.copyEmpty = false;

    data->installInfo.processed = data->installInfo.total;

    data->installInfo.isSrcDirectory = action_install_url_is_src_directory;
    data->installInfo.makeDstDirectory = action_install_url_make_dst_directory;

    data->installInfo.openSrc = action_install_url_open_src;
    data->installInfo.closeSrc = action_install_url_close_src;
    data->installInfo.getSrcSize = action_install_url_get_src_size;
    data->installInfo.readSrc = action_install_url_read_src;

    data->installInfo.openDst = action_install_url_open_dst;
    data->installInfo.closeDst = action_install_url_close_dst;
    data->installInfo.writeDst = action_install_url_write_dst;

    data->installInfo.suspendCopy = action_install_url_suspend_copy;
    data->installInfo.restoreCopy = action_install_url_restore_copy;

    data->installInfo.suspend = action_install_url_suspend;
    data->installInfo.restore = action_install_url_restore;

    data->installInfo.error = action_install_url_error;

    data->installInfo.finished = true;

    prompt_display_yes_no("Confirmation", confirmMessage, COLOR_TEXT, data, action_install_url_draw_top, action_install_url_confirm_onresponse);
}