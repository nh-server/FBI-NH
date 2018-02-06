#include <malloc.h>
#include <string.h>

#include <3ds.h>

#include "capturecam.h"
#include "task.h"
#include "../util.h"
#include "../../libs/quirc/quirc.h"

#define EVENT_CANCEL 0
#define EVENT_RECV 1
#define EVENT_BUFFER_ERROR 2

#define EVENT_COUNT 3

typedef struct {
    capture_cam_data* data;

    struct quirc* qrContext;
} capture_cam_internal_data;

static void task_capture_cam_thread(void* arg) {
    capture_cam_internal_data* internalData = (capture_cam_internal_data*) arg;
    capture_cam_data* data = internalData->data;

    data->qrReady = false;

    Handle events[EVENT_COUNT] = {0};
    events[EVENT_CANCEL] = data->cancelEvent;

    Result res = 0;

    u32 bufferSize = data->width * data->height * sizeof(u16);
    u16* buffer = (u16*) calloc(1, bufferSize);
    if(buffer != NULL) {
        if(R_SUCCEEDED(res = camInit())) {
            u32 cam = data->camera == CAMERA_OUTER ? SELECT_OUT1 : SELECT_IN1;

            if(R_SUCCEEDED(res = CAMU_SetSize(cam, SIZE_CTR_TOP_LCD, CONTEXT_A))
               && R_SUCCEEDED(res = CAMU_SetOutputFormat(cam, OUTPUT_RGB_565, CONTEXT_A))
               && R_SUCCEEDED(res = CAMU_SetFrameRate(cam, FRAME_RATE_30))
               && R_SUCCEEDED(res = CAMU_SetNoiseFilter(cam, true))
               && R_SUCCEEDED(res = CAMU_SetAutoExposure(cam, true))
               && R_SUCCEEDED(res = CAMU_SetAutoWhiteBalance(cam, true))
               && R_SUCCEEDED(res = CAMU_Activate(cam))) {
                u32 transferUnit = 0;

                if(R_SUCCEEDED(res = CAMU_GetBufferErrorInterruptEvent(&events[EVENT_BUFFER_ERROR], PORT_CAM1))
                   && R_SUCCEEDED(res = CAMU_SetTrimming(PORT_CAM1, true))
                   && R_SUCCEEDED(res = CAMU_SetTrimmingParamsCenter(PORT_CAM1, data->width, data->height, 400, 240))
                   && R_SUCCEEDED(res = CAMU_GetMaxBytes(&transferUnit, data->width, data->height))
                   && R_SUCCEEDED(res = CAMU_SetTransferBytes(PORT_CAM1, transferUnit, data->width, data->height))
                   && R_SUCCEEDED(res = CAMU_ClearBuffer(PORT_CAM1))
                   && R_SUCCEEDED(res = CAMU_SetReceiving(&events[EVENT_RECV], buffer, PORT_CAM1, bufferSize, (s16) transferUnit))
                   && R_SUCCEEDED(res = CAMU_StartCapture(PORT_CAM1))) {
                    bool cancelRequested = false;
                    while(!task_is_quit_all() && !cancelRequested && R_SUCCEEDED(res)) {
                        svcWaitSynchronization(task_get_pause_event(), U64_MAX);

                        s32 index = 0;
                        if(R_SUCCEEDED(res = svcWaitSynchronizationN(&index, events, EVENT_COUNT, false, U64_MAX))) {
                            switch(index) {
                                case EVENT_CANCEL:
                                    cancelRequested = true;
                                    break;
                                case EVENT_RECV:
                                    svcCloseHandle(events[EVENT_RECV]);
                                    events[EVENT_RECV] = 0;

                                    svcWaitSynchronization(data->mutex, U64_MAX);
                                    memcpy(data->buffer, buffer, bufferSize);
                                    GSPGPU_FlushDataCache(data->buffer, bufferSize);
                                    svcReleaseMutex(data->mutex);

                                    if(data->scanQR) {
                                        int w = 0;
                                        int h = 0;
                                        uint8_t* qrBuf = quirc_begin(internalData->qrContext, &w, &h);

                                        for(int x = 0; x < w; x++) {
                                            for(int y = 0; y < h; y++) {
                                                u16 px = buffer[y * data->width + x];
                                                qrBuf[y * w + x] = (u8) (((((px >> 11) & 0x1F) << 3) + (((px >> 5) & 0x3F) << 2) + ((px & 0x1F) << 3)) / 3);
                                            }
                                        }

                                        quirc_end(internalData->qrContext);

                                        int qrCount = quirc_count(internalData->qrContext);
                                        for(int i = 0; i < qrCount; i++) {
                                            struct quirc_code qrCode;
                                            quirc_extract(internalData->qrContext, i, &qrCode);

                                            struct quirc_data qrData;
                                            if(quirc_decode(&qrCode, &qrData) == 0) {
                                                svcWaitSynchronization(data->mutex, U64_MAX);

                                                data->qrReady = true;
                                                memcpy(data->qrData, qrData.payload, sizeof(data->qrData));

                                                svcReleaseMutex(data->mutex);
                                            }
                                        }
                                    }

                                    res = CAMU_SetReceiving(&events[EVENT_RECV], buffer, PORT_CAM1, bufferSize, (s16) transferUnit);
                                    break;
                                case EVENT_BUFFER_ERROR:
                                    svcCloseHandle(events[EVENT_RECV]);
                                    events[EVENT_RECV] = 0;

                                    if(R_SUCCEEDED(res = CAMU_ClearBuffer(PORT_CAM1))
                                       && R_SUCCEEDED(res = CAMU_SetReceiving(&events[EVENT_RECV], buffer, PORT_CAM1, bufferSize, (s16) transferUnit))) {
                                        res = CAMU_StartCapture(PORT_CAM1);
                                    }

                                    break;
                                default:
                                    break;
                            }
                        }
                    }

                    CAMU_StopCapture(PORT_CAM1);

                    bool busy = false;
                    while(R_SUCCEEDED(CAMU_IsBusy(&busy, PORT_CAM1)) && busy) {
                        svcSleepThread(1000000);
                    }

                    CAMU_ClearBuffer(PORT_CAM1);
                }

                CAMU_Activate(SELECT_NONE);
            }

            camExit();
        }

        free(buffer);
    } else {
        res = R_FBI_OUT_OF_MEMORY;
    }

    for(int i = 0; i < EVENT_COUNT; i++) {
        if(events[i] != 0) {
            svcCloseHandle(events[i]);
            events[i] = 0;
        }
    }

    quirc_destroy(internalData->qrContext);
    free(internalData);

    svcCloseHandle(data->mutex);

    data->result = res;
    data->finished = true;
}

Result task_capture_cam(capture_cam_data* data) {
    if(data == NULL || data->buffer == NULL || data->width <= 0 || data->width > 640 || data->height <= 0 || data->height > 480) {
        return R_FBI_INVALID_ARGUMENT;
    }

    capture_cam_internal_data* internalData = (capture_cam_internal_data*) calloc(1, sizeof(capture_cam_internal_data));
    if(internalData == NULL) {
        return R_FBI_OUT_OF_MEMORY;
    }

    data->mutex = 0;

    data->finished = false;
    data->result = 0;
    data->cancelEvent = 0;

    internalData->data = data;

    Result res = 0;

    internalData->qrContext = quirc_new();
    if(internalData->qrContext != NULL && quirc_resize(internalData->qrContext, data->width, data->height) == 0) {
        if(R_SUCCEEDED(res = svcCreateEvent(&data->cancelEvent, RESET_STICKY)) && R_SUCCEEDED(res = svcCreateMutex(&data->mutex, false))) {
            if(threadCreate(task_capture_cam_thread, internalData, 0x10000, 0x1A, 0, true) == NULL) {
                res = R_FBI_THREAD_CREATE_FAILED;
            }
        }
    } else {
        res = R_FBI_QR_INIT_FAILED;
    }


    if(R_FAILED(res)) {
        data->finished = true;

        if(internalData->qrContext != NULL) {
            quirc_destroy(internalData->qrContext);
            internalData->qrContext = NULL;
        }

        free(internalData);

        if(data->cancelEvent != 0) {
            svcCloseHandle(data->cancelEvent);
            data->cancelEvent = 0;
        }

        if(data->mutex != 0) {
            svcCloseHandle(data->mutex);
            data->mutex = 0;
        }
    }

    return res;
}