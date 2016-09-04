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

#define URL_MAX 1024
#define URLS_MAX 128

typedef struct {
    char urls[URLS_MAX][URL_MAX];

    bool cdn;
    bool cdnDecided;

    u32 responseCode;
    bool ticket;
    u64 currTitleId;
    volatile bool n3dsContinue;
    ticket_info ticketInfo;

    data_op_data installInfo;
} url_install_data;

static void action_url_install_cdn_check_onresponse(ui_view* view, void* data, bool response) {
    url_install_data* installData = (url_install_data*) data;

    installData->cdn = response;
    installData->cdnDecided = true;
}

static void action_url_install_n3ds_onresponse(ui_view* view, void* data, bool response) {
    ((url_install_data*) data)->n3dsContinue = response;
}

static Result action_url_install_is_src_directory(void* data, u32 index, bool* isDirectory) {
    *isDirectory = false;
    return 0;
}

static Result action_url_install_make_dst_directory(void* data, u32 index) {
    return 0;
}

static Result action_url_install_open_src(void* data, u32 index, u32* handle) {
    url_install_data* installData = (url_install_data*) data;

    Result res = 0;

    httpcContext* context = (httpcContext*) calloc(1, sizeof(httpcContext));
    if(context != NULL) {
        if(R_SUCCEEDED(res = httpcOpenContext(context, HTTPC_METHOD_GET, installData->urls[index], 1))) {
            char userAgent[128];
            snprintf(userAgent, sizeof(userAgent), "Mozilla/5.0 (Nintendo 3DS; Mobile; rv:10.0) Gecko/20100101 FBI/%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO);

            if(R_SUCCEEDED(res = httpcSetSSLOpt(context, SSLCOPT_DisableVerify)) && R_SUCCEEDED(res = httpcAddRequestHeaderField(context, "User-Agent", userAgent)) && R_SUCCEEDED(res = httpcBeginRequest(context)) && R_SUCCEEDED(res = httpcGetResponseStatusCode(context, &installData->responseCode))) {
                if(installData->responseCode == 200) {
                    *handle = (u32) context;
                } else if(installData->responseCode == 301 || installData->responseCode == 302 || installData->responseCode == 303) {
                    memset(installData->urls[index], '\0', URL_MAX);
                    if(R_SUCCEEDED(res = httpcGetResponseHeader(context, "Location", installData->urls[index], URL_MAX))) {
                        httpcCloseContext(context);
                        free(context);

                        return action_url_install_open_src(data, index, handle);
                    }
                } else {
                    res = R_FBI_HTTP_RESPONSE_CODE;
                }
            }

            if(R_FAILED(res)) {
                httpcCloseContext(context);
            }
        }

        if(R_FAILED(res)) {
            free(context);
        }
    } else {
        res = R_FBI_OUT_OF_MEMORY;
    }

    return res;
}

static Result action_url_install_close_src(void* data, u32 index, bool succeeded, u32 handle) {
    return httpcCloseContext((httpcContext*) handle);
}

static Result action_url_install_get_src_size(void* data, u32 handle, u64* size) {
    u32 downloadSize = 0;
    Result res = httpcGetDownloadSizeState((httpcContext*) handle, NULL, &downloadSize);

    *size = downloadSize;
    return res;
}

static Result action_url_install_read_src(void* data, u32 handle, u32* bytesRead, void* buffer, u64 offset, u32 size) {
    Result res = httpcDownloadData((httpcContext*) handle, buffer, size, bytesRead);
    return res != HTTPC_RESULTCODE_DOWNLOADPENDING ? res : 0;
}

static Result action_url_install_open_dst(void* data, u32 index, void* initialReadBlock, u64 size, u32* handle) {
    url_install_data* installData = (url_install_data*) data;

    Result res = 0;

    installData->responseCode = 0;
    installData->ticket = false;
    installData->currTitleId = 0;
    installData->n3dsContinue = false;
    memset(&installData->ticketInfo, 0, sizeof(installData->ticketInfo));

    if(*(u16*) initialReadBlock == 0x0100) {
        if(!installData->cdnDecided) {
            ui_view* view = prompt_display("Optional", "Install ticket titles from CDN?", COLOR_TEXT, true, data, NULL, action_url_install_cdn_check_onresponse);
            if(view != NULL) {
                svcWaitSynchronization(view->active, U64_MAX);
            }
        }

        installData->ticket = true;
        installData->ticketInfo.titleId = util_get_ticket_title_id((u8*) initialReadBlock);

        AM_DeleteTicket(installData->ticketInfo.titleId);
        res = AM_InstallTicketBegin(handle);
    } else if(*(u16*) initialReadBlock == 0x2020) {
        u64 titleId = util_get_cia_title_id((u8*) initialReadBlock);

        FS_MediaType dest = util_get_title_destination(titleId);

        bool n3ds = false;
        if(R_SUCCEEDED(APT_CheckNew3DS(&n3ds)) && !n3ds && ((titleId >> 28) & 0xF) == 2) {
            ui_view* view = prompt_display("Confirmation", "Title is intended for New 3DS systems.\nContinue?", COLOR_TEXT, true, data, NULL, action_url_install_n3ds_onresponse);
            if(view != NULL) {
                svcWaitSynchronization(view->active, U64_MAX);
            }

            if(!installData->n3dsContinue) {
                return R_FBI_WRONG_SYSTEM;
            }
        }

        // Deleting FBI before it reinstalls itself causes issues.
        if(((titleId >> 8) & 0xFFFFF) != 0xF8001) {
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

static Result action_url_install_close_dst(void* data, u32 index, bool succeeded, u32 handle) {
    url_install_data* installData = (url_install_data*) data;

    if(succeeded) {
        Result res = 0;

        if(installData->ticket) {
            res = AM_InstallTicketFinish(handle);

            if(R_SUCCEEDED(res) && installData->cdn) {
                volatile bool done = false;
                action_install_cdn_noprompt(&done, &installData->ticketInfo, false);

                while(!done) {
                    svcSleepThread(100000000);
                }
            }
        } else {
            if(R_SUCCEEDED(res = AM_FinishCiaInstall(handle))) {
                util_import_seed(installData->currTitleId);

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

static Result action_url_install_write_dst(void* data, u32 handle, u32* bytesWritten, void* buffer, u64 offset, u32 size) {
    return FSFILE_Write(handle, bytesWritten, offset, buffer, size, 0);
}

static Result action_url_install_suspend_copy(void* data, u32 index, u32* srcHandle, u32* dstHandle) {
    return 0;
}

static Result action_url_install_restore_copy(void* data, u32 index, u32* srcHandle, u32* dstHandle) {
    return 0;
}

static Result action_url_install_suspend(void* data, u32 index) {
    return 0;
}

static Result action_url_install_restore(void* data, u32 index) {
    return 0;
}

static bool action_url_install_error(void* data, u32 index, Result res) {
    url_install_data* installData = (url_install_data*) data;

    if(res == R_FBI_CANCELLED) {
        prompt_display("Failure", "Install cancelled.", COLOR_TEXT, false, NULL, NULL, NULL);
        return false;
    } else if(res != R_FBI_WRONG_SYSTEM) {
        char* url = installData->urls[index];

        ui_view* view = NULL;

        if(res == R_FBI_HTTP_RESPONSE_CODE) {
            if(strlen(url) > 38) {
                view = error_display(NULL, NULL, "Failed to install from URL.\n%.35s...\nHTTP server returned response code %d", url, installData->responseCode);
            } else {
                view = error_display(NULL, NULL, "Failed to install from URL.\n%.38s\nHTTP server returned response code %d", url, installData->responseCode);
            }
        } else {
            if(strlen(url) > 38) {
                view = error_display_res(NULL, NULL, res, "Failed to install from URL.\n%.35s...", url);
            } else {
                view = error_display_res(NULL, NULL, res, "Failed to install from URL.\n%.38s", url);
            }
        }

        if(view != NULL) {
            svcWaitSynchronization(view->active, U64_MAX);
        }
    }

    return index < installData->installInfo.total - 1;
}

static void action_url_install_install_update(ui_view* view, void* data, float* progress, char* text) {
    url_install_data* installData = (url_install_data*) data;

    if(installData->installInfo.finished) {
        ui_pop();
        info_destroy(view);

        if(R_SUCCEEDED(installData->installInfo.result)) {
            prompt_display("Success", "Install finished.", COLOR_TEXT, false, NULL, NULL, NULL);
        }

        return;
    }

    if(hidKeysDown() & KEY_B) {
        svcSignalEvent(installData->installInfo.cancelEvent);
    }

    *progress = installData->installInfo.currTotal != 0 ? (float) ((double) installData->installInfo.currProcessed / (double) installData->installInfo.currTotal) : 0;
    snprintf(text, PROGRESS_TEXT_MAX, "%lu / %lu\n%.2f %s / %.2f %s", installData->installInfo.processed, installData->installInfo.total, util_get_display_size(installData->installInfo.currProcessed), util_get_display_size_units(installData->installInfo.currProcessed), util_get_display_size(installData->installInfo.currTotal), util_get_display_size_units(installData->installInfo.currTotal));
}

static void action_url_install_confirm_onresponse(ui_view* view, void* data, bool response) {
    url_install_data* installData = (url_install_data*) data;

    if(response) {
        Result res = task_data_op(&installData->installInfo);
        if(R_SUCCEEDED(res)) {
            info_display("Installing From URL(s)", "Press B to cancel.", true, data, action_url_install_install_update, NULL);
        } else {
            error_display_res(NULL, NULL, res, "Failed to initiate installation.");
        }
    }
}

void action_url_install(const char* confirmMessage, const char* urls) {
    url_install_data* data = (url_install_data*) calloc(1, sizeof(url_install_data));
    if(data == NULL) {
        error_display(NULL, NULL, "Failed to allocate QR install data.");

        return;
    }

    data->installInfo.total = 0;

    size_t payloadLen = strlen(urls);
    if(payloadLen > 0) {
        const char* currStart = urls;
        while(data->installInfo.total < URLS_MAX && currStart - urls < payloadLen) {
            const char* currEnd = strchr(currStart, '\n');
            if(currEnd == NULL) {
                currEnd = urls + payloadLen;
            }

            u32 len = currEnd - currStart;

            if((len < 7 || strncmp(currStart, "http://", 7) != 0) && (len < 8 || strncmp(currStart, "https://", 8) != 0)) {
                if(len > URL_MAX - 7) {
                    len = URL_MAX - 7;
                }

                strncpy(data->urls[data->installInfo.total], "http://", 7);
                strncpy(&data->urls[data->installInfo.total][7], currStart, len);
            } else {
                if(len > URL_MAX) {
                    len = URL_MAX;
                }

                strncpy(data->urls[data->installInfo.total], currStart, len);
            }

            data->installInfo.total++;
            currStart = currEnd + 1;
        }
    }

    data->cdn = false;
    data->cdnDecided = false;

    data->responseCode = 0;
    data->ticket = false;
    data->currTitleId = 0;
    data->n3dsContinue = false;
    memset(&data->ticketInfo, 0, sizeof(data->ticketInfo));

    data->installInfo.data = data;

    data->installInfo.op = DATAOP_COPY;

    data->installInfo.copyBufferSize = 256 * 1024;
    data->installInfo.copyEmpty = false;

    data->installInfo.isSrcDirectory = action_url_install_is_src_directory;
    data->installInfo.makeDstDirectory = action_url_install_make_dst_directory;

    data->installInfo.openSrc = action_url_install_open_src;
    data->installInfo.closeSrc = action_url_install_close_src;
    data->installInfo.getSrcSize = action_url_install_get_src_size;
    data->installInfo.readSrc = action_url_install_read_src;

    data->installInfo.openDst = action_url_install_open_dst;
    data->installInfo.closeDst = action_url_install_close_dst;
    data->installInfo.writeDst = action_url_install_write_dst;

    data->installInfo.suspendCopy = action_url_install_suspend_copy;
    data->installInfo.restoreCopy = action_url_install_restore_copy;

    data->installInfo.suspend = action_url_install_suspend;
    data->installInfo.restore = action_url_install_restore;

    data->installInfo.error = action_url_install_error;

    data->installInfo.finished = true;

    prompt_display("Confirmation", confirmMessage, COLOR_TEXT, true, data, NULL, action_url_install_confirm_onresponse);
}