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
#include "../../quirc/quirc_internal.h"

#define IMAGE_WIDTH 400
#define IMAGE_HEIGHT 240

#define URL_MAX 1024
#define URLS_MAX 128

typedef struct {
    struct quirc* qrContext;
    u32 tex;

    bool capturing;
    capture_cam_data captureInfo;
} qr_install_data;

static void qrinstall_free_data(qr_install_data* data) {
    if(!data->captureInfo.finished) {
        svcSignalEvent(data->captureInfo.cancelEvent);
        while(!data->captureInfo.finished) {
            svcSleepThread(1000000);
        }
    }

    data->capturing = false;

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

    if(!qrInstallData->capturing) {
        Result capRes = task_capture_cam(&qrInstallData->captureInfo);
        if(R_FAILED(capRes)) {
            ui_pop();
            info_destroy(view);

            error_display_res(NULL, NULL, capRes, "Failed to start camera capture.");

            qrinstall_free_data(qrInstallData);
            return;
        } else {
            qrInstallData->capturing = true;
        }
    }

    if(qrInstallData->captureInfo.finished) {
        ui_pop();
        info_destroy(view);

        if(R_FAILED(qrInstallData->captureInfo.result)) {
            error_display_res(NULL, NULL, qrInstallData->captureInfo.result, "Error while capturing camera frames.");
        }

        qrinstall_free_data(qrInstallData);

        return;
    }

    if(hidKeysDown() & KEY_X) {
        SwkbdState swkbd;
        swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, -1);
        swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY, 0, 0);
        swkbdSetFeatures(&swkbd, SWKBD_MULTILINE);
        swkbdSetHintText(&swkbd, "Enter URL(s)");

        char textBuf[1024];
        if(swkbdInputText(&swkbd, textBuf, sizeof(textBuf)) == SWKBD_BUTTON_CONFIRM) {
            if(!qrInstallData->captureInfo.finished) {
                svcSignalEvent(qrInstallData->captureInfo.cancelEvent);
                while(!qrInstallData->captureInfo.finished) {
                    svcSleepThread(1000000);
                }
            }

            qrInstallData->capturing = false;
            memset(qrInstallData->captureInfo.buffer, 0, IMAGE_WIDTH * IMAGE_HEIGHT * sizeof(u16));

            action_url_install("Install from the provided URL(s)?", textBuf);
            return;
        }
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
            if(!qrInstallData->captureInfo.finished) {
                svcSignalEvent(qrInstallData->captureInfo.cancelEvent);
                while(!qrInstallData->captureInfo.finished) {
                    svcSleepThread(1000000);
                }
            }

            qrInstallData->capturing = false;
            memset(qrInstallData->captureInfo.buffer, 0, IMAGE_WIDTH * IMAGE_HEIGHT * sizeof(u16));

            action_url_install("Install from the scanned QR code?", (const char*) qrData.payload);
            return;
        }
    }

    snprintf(text, PROGRESS_TEXT_MAX, "Waiting for QR code...");
}

void qrinstall_open() {
    qr_install_data* data = (qr_install_data*) calloc(1, sizeof(qr_install_data));
    if(data == NULL) {
        error_display(NULL, NULL, "Failed to allocate QR install data.");

        return;
    }

    data->tex = 0;

    data->capturing = false;

    data->captureInfo.width = IMAGE_WIDTH;
    data->captureInfo.height = IMAGE_HEIGHT;

    data->captureInfo.finished = true;

    data->qrContext = quirc_new();
    if(data->qrContext == NULL) {
        error_display(NULL, NULL, "Failed to create QR context.");

        qrinstall_free_data(data);
        return;
    }

    if(quirc_resize(data->qrContext, IMAGE_WIDTH, IMAGE_HEIGHT) != 0) {
        error_display(NULL, NULL, "Failed to resize QR context.");

        qrinstall_free_data(data);
        return;
    }

    data->captureInfo.buffer = (u16*) calloc(1, IMAGE_WIDTH * IMAGE_HEIGHT * sizeof(u16));
    if(data->captureInfo.buffer == NULL) {
        error_display(NULL, NULL, "Failed to create image buffer.");

        qrinstall_free_data(data);
        return;
    }

    info_display("QR Code Install", "B: Return, X: Enter URL(s)", false, data, qrinstall_wait_update, qrinstall_wait_draw_top);
}
