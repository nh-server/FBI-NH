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

static bool qrinstall_get_last_urls(char* out, size_t size) {
    if(out == NULL || size == 0) {
        return false;
    }

    Handle file = 0;
    if(R_FAILED(FSUSER_OpenFileDirectly(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, "/fbi/lasturls"), FS_OPEN_READ, 0))) {
        return false;
    }

    u32 bytesRead = 0;
    FSFILE_Read(file, &bytesRead, 0, out, size - 1);
    out[bytesRead] = '\0';

    FSFILE_Close(file);

    return bytesRead != 0;
}

static Result qrinstall_set_last_urls(const char* urls) {
    Result res = 0;

    FS_Archive sdmcArchive = 0;
    if(R_SUCCEEDED(res = FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, "")))) {
        FS_Path path = fsMakePath(PATH_ASCII, "/fbi/lasturls");
        if(urls == NULL || strlen(urls) == 0) {
            res = FSUSER_DeleteFile(sdmcArchive, path);
        } else if(R_SUCCEEDED(res = util_ensure_dir(sdmcArchive, "/fbi/"))) {
            Handle file = 0;
            if(R_SUCCEEDED(res = FSUSER_OpenFile(&file, sdmcArchive, path, FS_OPEN_WRITE | FS_OPEN_CREATE, 0))) {
                u32 bytesWritten = 0;
                res = FSFILE_Write(file, &bytesWritten, 0, urls, strlen(urls), FS_WRITE_FLUSH | FS_WRITE_UPDATE_TIME);

                Result closeRes = FSFILE_Close(file);
                if(R_SUCCEEDED(res)) {
                    res = closeRes;
                }
            }
        }

        Result closeRes = FSUSER_CloseArchive(sdmcArchive);
        if(R_SUCCEEDED(res)) {
            res = closeRes;
        }
    }

    return res;
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

    if(hidKeysDown() & KEY_SELECT) {
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

            qrinstall_set_last_urls(textBuf);

            action_url_install("Install from the provided URL(s)?", textBuf);
            return;
        }
    }

    if(hidKeysDown() & KEY_X) {
        char textBuf[4096];
        if(qrinstall_get_last_urls(textBuf, sizeof(textBuf))) {
            action_url_install("Install from the last entered URL(s)?", textBuf);
        } else {
            prompt_display("Failure", "No previously entered URL(s) could be found.", COLOR_TEXT, false, NULL, NULL, NULL);
        }

        return;
    }

    if(hidKeysDown() & KEY_Y) {
        Result forgetRes = qrinstall_set_last_urls(NULL);
        if(R_SUCCEEDED(forgetRes)) {
            prompt_display("Success", "Last URL(s) forgotten.", COLOR_TEXT, false, NULL, NULL, NULL);
        } else {
            error_display_res(NULL, NULL, forgetRes, "Failed to forget last URL(s).");
        }

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
            if(!qrInstallData->captureInfo.finished) {
                svcSignalEvent(qrInstallData->captureInfo.cancelEvent);
                while(!qrInstallData->captureInfo.finished) {
                    svcSleepThread(1000000);
                }
            }

            qrInstallData->capturing = false;
            memset(qrInstallData->captureInfo.buffer, 0, IMAGE_WIDTH * IMAGE_HEIGHT * sizeof(u16));

            qrinstall_set_last_urls((const char*) qrData.payload);

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

    info_display("QR Code Install", "B: Return, X: Repeat, Y: Forget, SELECT: URL(s)", false, data, qrinstall_wait_update, qrinstall_wait_draw_top);
}
