#include <malloc.h>
#include <string.h>

#include <3ds.h>

#include "task.h"
#include "../../list.h"
#include "../../error.h"

#define EVENT_CANCEL 0
#define EVENT_RECV 1
#define EVENT_BUFFER_ERROR 2

#define EVENT_COUNT 3

static void task_capture_cam_thread(void* arg) {
    capture_cam_data* data = (capture_cam_data*) arg;

    Handle events[EVENT_COUNT] = {0};
    events[EVENT_CANCEL] = data->cancelEvent;

    Result res = 0;

    u32 bufferSize = data->width * data->height * sizeof(u16);
    u16* buffer = (u16*) calloc(1, bufferSize);
    if(buffer != NULL) {
        if(R_SUCCEEDED(res = camInit())) {
            if(R_SUCCEEDED(res = CAMU_SetSize(SELECT_OUT1, SIZE_CTR_TOP_LCD, CONTEXT_A))
               && R_SUCCEEDED(res = CAMU_SetOutputFormat(SELECT_OUT1, OUTPUT_RGB_565, CONTEXT_A))
               && R_SUCCEEDED(res = CAMU_SetFrameRate(SELECT_OUT1, FRAME_RATE_30))
               && R_SUCCEEDED(res = CAMU_SetNoiseFilter(SELECT_OUT1, true))
               && R_SUCCEEDED(res = CAMU_SetAutoExposure(SELECT_OUT1, true))
               && R_SUCCEEDED(res = CAMU_SetAutoWhiteBalance(SELECT_OUT1, true))
               && R_SUCCEEDED(res = CAMU_Activate(SELECT_OUT1))) {
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

    svcCloseHandle(data->mutex);

    data->result = res;
    data->finished = true;
}

Result task_capture_cam(capture_cam_data* data) {
    if(data == NULL || data->buffer == NULL || data->width <= 0 || data->width > 640 || data->height <= 0 || data->height > 480) {
        return R_FBI_INVALID_ARGUMENT;
    }

    data->mutex = 0;

    data->finished = false;
    data->result = 0;
    data->cancelEvent = 0;

    Result res = 0;
    if(R_SUCCEEDED(res = svcCreateEvent(&data->cancelEvent, RESET_STICKY)) && R_SUCCEEDED(res = svcCreateMutex(&data->mutex, false))) {
        if(threadCreate(task_capture_cam_thread, data, 0x10000, 0x1A, 1, true) == NULL) {
            res = R_FBI_THREAD_CREATE_FAILED;
        }
    }

    if(R_FAILED(res)) {
        data->finished = true;

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