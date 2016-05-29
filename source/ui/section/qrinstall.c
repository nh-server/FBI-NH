#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <3ds.h>

#include "section.h"
#include "action/action.h"
#include "task/task.h"
#include "../error.h"
#include "../info.h"
#include "../prompt.h"
#include "../ui.h"
#include "../../core/screen.h"
#include "../../core/util.h"
#include "../../quirc/quirc_internal.h"

#define IMAGE_WIDTH 400
#define IMAGE_HEIGHT 240

#define URL_MAX 1024
#define URLS_MAX 128

typedef struct {
    struct quirc* qrContext;
    char urls[URLS_MAX][URL_MAX];

    u32 tex;

    u32 responseCode;
    u64 currTitleId;
    bool ticket;

    bool cdn;
    ticket_info ticketInfo;

    capture_cam_data captureInfo;
    data_op_data installInfo;
} qr_install_data;

static Result qrinstall_is_src_directory(void* data, u32 index, bool* isDirectory) {
    *isDirectory = false;
    return 0;
}

static Result qrinstall_make_dst_directory(void* data, u32 index) {
    return 0;
}

static Result qrinstall_open_src(void* data, u32 index, u32* handle) {
    qr_install_data* qrInstallData = (qr_install_data*) data;

    Result res = 0;

    httpcContext* context = (httpcContext*) calloc(1, sizeof(httpcContext));
    if(context != NULL) {
        if(R_SUCCEEDED(res = httpcOpenContext(context, HTTPC_METHOD_GET, qrInstallData->urls[index], 1))) {
            httpcSetSSLOpt(context, SSLCOPT_DisableVerify);
            if(R_SUCCEEDED(res = httpcBeginRequest(context)) && R_SUCCEEDED(res = httpcGetResponseStatusCode(context, &qrInstallData->responseCode, 0))) {
                if(qrInstallData->responseCode == 200) {
                    *handle = (u32) context;
                } else if(qrInstallData->responseCode == 301 || qrInstallData->responseCode == 302 || qrInstallData->responseCode == 303) {
                    if(R_SUCCEEDED(res = httpcGetResponseHeader(context, "Location", qrInstallData->urls[index], URL_MAX))) {
                        httpcCloseContext(context);
                        free(context);

                        return qrinstall_open_src(data, index, handle);
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

static Result qrinstall_close_src(void* data, u32 index, bool succeeded, u32 handle) {
    return httpcCloseContext((httpcContext*) handle);
}

static Result qrinstall_get_src_size(void* data, u32 handle, u64* size) {
    u32 downloadSize = 0;
    Result res = httpcGetDownloadSizeState((httpcContext*) handle, NULL, &downloadSize);

    *size = downloadSize;
    return res;
}

static Result qrinstall_read_src(void* data, u32 handle, u32* bytesRead, void* buffer, u64 offset, u32 size) {
    Result res = httpcDownloadData((httpcContext*) handle, buffer, size, bytesRead);
    return res != HTTPC_RESULTCODE_DOWNLOADPENDING ? res : 0;
}

static Result qrinstall_open_dst(void* data, u32 index, void* initialReadBlock, u32* handle) {
    qr_install_data* qrInstallData = (qr_install_data*) data;

    qrInstallData->ticket = *(u16*) initialReadBlock == 0x0100;

    Result res = 0;

    if(qrInstallData->ticket) {
        qrInstallData->ticketInfo.titleId = util_get_ticket_title_id((u8*) initialReadBlock);

        AM_DeleteTicket(qrInstallData->ticketInfo.titleId);
        res = AM_InstallTicketBegin(handle);
    } else {
        u64 titleId = util_get_cia_title_id((u8*) initialReadBlock);

        FS_MediaType dest = ((titleId >> 32) & 0x8010) != 0 ? MEDIATYPE_NAND : MEDIATYPE_SD;

        u8 n3ds = false;
        if(R_SUCCEEDED(APT_CheckNew3DS(&n3ds)) && !n3ds && ((titleId >> 28) & 0xF) == 2) {
            return R_FBI_WRONG_SYSTEM;
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
            qrInstallData->currTitleId = titleId;
        }
    }

    return res;
}

static Result qrinstall_close_dst(void* data, u32 index, bool succeeded, u32 handle) {
    qr_install_data* qrInstallData = (qr_install_data*) data;

    if(succeeded) {
        Result res = 0;

        if(qrInstallData->ticket) {
            res = AM_InstallTicketFinish(handle);

            if(R_SUCCEEDED(res) && qrInstallData->cdn) {
                volatile bool done = false;
                action_install_cdn_noprompt(&done, &qrInstallData->ticketInfo, false);

                while(!done) {
                    svcSleepThread(100000000);
                }
            }
        } else {
            if(R_SUCCEEDED(res = AM_FinishCiaInstall(handle))) {
                util_import_seed(qrInstallData->currTitleId);

                if(qrInstallData->currTitleId == 0x0004013800000002 || qrInstallData->currTitleId == 0x0004013820000002) {
                    res = AM_InstallFirm(qrInstallData->currTitleId);
                }
            }
        }

        return res;
    } else {
        if(qrInstallData->ticket) {
            return AM_InstallTicketAbort(handle);
        } else {
            return AM_CancelCIAInstall(handle);
        }
    }
}

static Result qrinstall_write_dst(void* data, u32 handle, u32* bytesWritten, void* buffer, u64 offset, u32 size) {
    return FSFILE_Write(handle, bytesWritten, offset, buffer, size, 0);
}

static bool qrinstall_error(void* data, u32 index, Result res) {
    qr_install_data* qrInstallData = (qr_install_data*) data;

    if(res == R_FBI_CANCELLED) {
        prompt_display("Failure", "Install cancelled.", COLOR_TEXT, false, NULL, NULL, NULL, NULL);
        return false;
    } else {
        char* url = qrInstallData->urls[index];

        volatile bool dismissed = false;
        if(res == R_FBI_HTTP_RESPONSE_CODE) {
            if(strlen(url) > 38) {
                error_display(&dismissed, NULL, NULL, "Failed to install from QR code.\n%.35s...\nHTTP server returned response code %d", url, qrInstallData->responseCode);
            } else {
                error_display(&dismissed, NULL, NULL, "Failed to install from QR code.\n%.38s\nHTTP server returned response code %d", url, qrInstallData->responseCode);
            }
        } else {
            if(strlen(url) > 38) {
                error_display_res(&dismissed, NULL, NULL, res, "Failed to install from QR code.\n%.35s...", url);
            } else {
                error_display_res(&dismissed, NULL, NULL, res, "Failed to install from QR code.\n%.38s", url);
            }
        }

        while(!dismissed) {
            svcSleepThread(1000000);
        }
    }

    return index < qrInstallData->installInfo.total - 1;
}

static void qrinstall_install_update(ui_view* view, void* data, float* progress, char* text) {
    qr_install_data* qrInstallData = (qr_install_data*) data;

    if(qrInstallData->installInfo.finished) {
        ui_pop();
        info_destroy(view);

        if(R_SUCCEEDED(qrInstallData->installInfo.result)) {
            prompt_display("Success", "Install finished.", COLOR_TEXT, false, data, NULL, NULL, NULL);
        }

        return;
    }

    if(hidKeysDown() & KEY_B) {
        svcSignalEvent(qrInstallData->installInfo.cancelEvent);
    }

    *progress = qrInstallData->installInfo.currTotal != 0 ? (float) ((double) qrInstallData->installInfo.currProcessed / (double) qrInstallData->installInfo.currTotal) : 0;
    snprintf(text, PROGRESS_TEXT_MAX, "%lu / %lu\n%.2f MiB / %.2f MiB", qrInstallData->installInfo.processed, qrInstallData->installInfo.total, qrInstallData->installInfo.currProcessed / 1024.0 / 1024.0, qrInstallData->installInfo.currTotal / 1024.0 / 1024.0);
}

static void qrinstall_cdn_check_onresponse(ui_view* view, void* data, bool response) {
    qr_install_data* qrInstallData = (qr_install_data*) data;

    qrInstallData->cdn = response;

    Result res = task_data_op(&qrInstallData->installInfo);
    if(R_SUCCEEDED(res)) {
        info_display("Installing From QR Code", "Press B to cancel.", true, data, qrinstall_install_update, NULL);
    } else {
        error_display_res(NULL, NULL, NULL, res, "Failed to initiate installation.");
    }
}

static void qrinstall_confirm_onresponse(ui_view* view, void* data, bool response) {
    if(response) {
        prompt_display("Optional", "Install ticket titles from CDN?", COLOR_TEXT, true, data, NULL, NULL, qrinstall_cdn_check_onresponse);
    }
}

static void qrinstall_free_data(qr_install_data* data) {
    if(!data->installInfo.finished) {
        svcSignalEvent(data->installInfo.cancelEvent);
        while(!data->installInfo.finished) {
            svcSleepThread(1000000);
        }
    }

    if(!data->captureInfo.finished) {
        svcSignalEvent(data->captureInfo.cancelEvent);
        while(!data->captureInfo.finished) {
            svcSleepThread(1000000);
        }
    }

    if(data->captureInfo.buffer != NULL) {
        free(data->captureInfo.buffer);
        data->captureInfo.buffer = NULL;
    }

    if(data->tex != 0) {
        screen_unload_texture(data->tex);
        data->tex = 0;
    }

    if(data->qrContext != NULL) {
        quirc_destroy(data->qrContext);
        data->qrContext = NULL;
    }

    free(data);
}

static void qrinstall_wait_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    qr_install_data* qrInstallData = (qr_install_data*) data;

    if(qrInstallData->tex != 0) {
        screen_draw_texture(qrInstallData->tex, 0, 0, IMAGE_WIDTH, IMAGE_HEIGHT);
    }
}

static void qrinstall_wait_update(ui_view* view, void* data, float* progress, char* text) {
    qr_install_data* qrInstallData = (qr_install_data*) data;

    if(hidKeysDown() & KEY_B) {
        ui_pop();
        info_destroy(view);

        qrinstall_free_data(qrInstallData);

        return;
    }

    if(qrInstallData->captureInfo.finished) {
        ui_pop();
        info_destroy(view);

        if(R_FAILED(qrInstallData->captureInfo.result)) {
            error_display_res(NULL, NULL, NULL, qrInstallData->captureInfo.result, "Error while capturing camera frames.");
        }

        qrinstall_free_data(qrInstallData);

        return;
    }

    if(qrInstallData->tex != 0) {
        screen_unload_texture(qrInstallData->tex);
        qrInstallData->tex = 0;
    }

    int w = 0;
    int h = 0;
    uint8_t* qrBuf = quirc_begin(qrInstallData->qrContext, &w, &h);

    svcWaitSynchronization(qrInstallData->captureInfo.mutex, U64_MAX);

    qrInstallData->tex = screen_load_texture_auto(qrInstallData->captureInfo.buffer, IMAGE_WIDTH * IMAGE_HEIGHT * sizeof(u16), IMAGE_WIDTH, IMAGE_HEIGHT, GPU_RGB565, false);

    for(int x = 0; x < w; x++) {
        for(int y = 0; y < h; y++) {
            u16 px = qrInstallData->captureInfo.buffer[y * IMAGE_WIDTH + x];
            qrBuf[y * w + x] = (u8) (((((px >> 11) & 0x1F) << 3) + (((px >> 5) & 0x3F) << 2) + ((px & 0x1F) << 3)) / 3);
        }
    }

    svcReleaseMutex(qrInstallData->captureInfo.mutex);

    quirc_end(qrInstallData->qrContext);

    int qrCount = quirc_count(qrInstallData->qrContext);
    for(int i = 0; i < qrCount; i++) {
        struct quirc_code qrCode;
        quirc_extract(qrInstallData->qrContext, i, &qrCode);

        struct quirc_data qrData;
        quirc_decode_error_t err = quirc_decode(&qrCode, &qrData);

        if(err == 0) {
            qrInstallData->installInfo.total = 0;

            size_t payloadLen = strlen((char*) qrData.payload);

            char* currStart = (char*) qrData.payload;
            while(qrInstallData->installInfo.total < URLS_MAX && currStart - (char*) qrData.payload < payloadLen) {
                char* currEnd = strchr(currStart, '\n');
                if(currEnd == NULL) {
                    currEnd = (char*) qrData.payload + payloadLen;
                }

                u32 len = currEnd - currStart;

                if((len < 7 || strncmp(currStart, "http://", 7) != 0) && (len < 8 || strncmp(currStart, "https://", 8) != 0)) {
                    if(len > URL_MAX - 7) {
                        len = URL_MAX - 7;
                    }

                    strncpy(qrInstallData->urls[qrInstallData->installInfo.total], "http://", 7);
                    strncpy(&qrInstallData->urls[qrInstallData->installInfo.total][7], currStart, len);
                } else {
                    if(len > URL_MAX) {
                        len = URL_MAX;
                    }

                    strncpy(qrInstallData->urls[qrInstallData->installInfo.total], currStart, len);
                }

                qrInstallData->installInfo.total++;
                currStart = currEnd + 1;
            }

            prompt_display("Confirmation", "Install from the scanned URL(s)?", COLOR_TEXT, true, data, NULL, NULL, qrinstall_confirm_onresponse);
        }
    }

    snprintf(text, PROGRESS_TEXT_MAX, "Waiting for QR code...");
}

void qrinstall_open() {
    qr_install_data* data = (qr_install_data*) calloc(1, sizeof(qr_install_data));
    if(data == NULL) {
        error_display(NULL, NULL, NULL, "Failed to allocate QR install data.");

        return;
    }

    data->tex = 0;

    data->responseCode = 0;
    data->currTitleId = 0;
    data->ticket = false;

    data->installInfo.data = data;

    data->installInfo.op = DATAOP_COPY;

    data->installInfo.copyEmpty = false;

    data->installInfo.total = 0;

    data->installInfo.isSrcDirectory = qrinstall_is_src_directory;
    data->installInfo.makeDstDirectory = qrinstall_make_dst_directory;

    data->installInfo.openSrc = qrinstall_open_src;
    data->installInfo.closeSrc = qrinstall_close_src;
    data->installInfo.getSrcSize = qrinstall_get_src_size;
    data->installInfo.readSrc = qrinstall_read_src;

    data->installInfo.openDst = qrinstall_open_dst;
    data->installInfo.closeDst = qrinstall_close_dst;
    data->installInfo.writeDst = qrinstall_write_dst;

    data->installInfo.error = qrinstall_error;

    data->installInfo.finished = true;

    data->captureInfo.width = IMAGE_WIDTH;
    data->captureInfo.height = IMAGE_HEIGHT;

    data->captureInfo.finished = true;

    data->qrContext = quirc_new();
    if(data->qrContext == NULL) {
        error_display(NULL, NULL, NULL, "Failed to create QR context.");

        qrinstall_free_data(data);
        return;
    }

    if(quirc_resize(data->qrContext, IMAGE_WIDTH, IMAGE_HEIGHT) != 0) {
        error_display(NULL, NULL, NULL, "Failed to resize QR context.");

        qrinstall_free_data(data);
        return;
    }

    data->captureInfo.buffer = (u16*) calloc(1, IMAGE_WIDTH * IMAGE_HEIGHT * sizeof(u16));
    if(data->captureInfo.buffer == NULL) {
        error_display(NULL, NULL, NULL, "Failed to create image buffer.");

        qrinstall_free_data(data);
        return;
    }

    Result capRes = task_capture_cam(&data->captureInfo);
    if(R_FAILED(capRes)) {
        error_display_res(NULL, NULL, NULL, capRes, "Failed to start camera capture.");

        qrinstall_free_data(data);
        return;
    }

    info_display("QR Code Install", "B: Return", false, data, qrinstall_wait_update, qrinstall_wait_draw_top);
}
