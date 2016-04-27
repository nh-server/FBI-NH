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

typedef struct {
    u16* buffer;
    s16 width;
    s16 height;

    Handle mutex;

    Handle cancelEvent;
} capture_cam_data;

static void task_capture_cam_thread(void* arg) {
    capture_cam_data* data = (capture_cam_data*) arg;

    Handle events[EVENT_COUNT] = {0};
    events[EVENT_CANCEL] = data->cancelEvent;

    Result res = 0;

    u32 bufferSize = data->width * data->height * sizeof(u16);
    u16* buffer = (u16*) calloc(1, bufferSize);
    if(buffer != NULL) {
        if(R_SUCCEEDED(res = camInit())) {
            if(R_SUCCEEDED(res = CAMU_SetSize(SELECT_OUT1, SIZE_VGA, CONTEXT_A))
               && R_SUCCEEDED(res = CAMU_SetOutputFormat(SELECT_OUT1, OUTPUT_RGB_565, CONTEXT_A))
               && R_SUCCEEDED(res = CAMU_SetFrameRate(SELECT_OUT1, FRAME_RATE_30))
               && R_SUCCEEDED(res = CAMU_SetNoiseFilter(SELECT_OUT1, true))
               && R_SUCCEEDED(res = CAMU_SetAutoExposure(SELECT_OUT1, true))
               && R_SUCCEEDED(res = CAMU_SetAutoWhiteBalance(SELECT_OUT1, true))
               && R_SUCCEEDED(res = CAMU_Activate(SELECT_OUT1))) {
                u32 transferUnit = 0;

                if(R_SUCCEEDED(res = CAMU_GetBufferErrorInterruptEvent(&events[EVENT_BUFFER_ERROR], PORT_CAM1))
                   && R_SUCCEEDED(res = CAMU_SetTrimming(PORT_CAM1, true))
                   && R_SUCCEEDED(res = CAMU_SetTrimmingParamsCenter(PORT_CAM1, data->width, data->height, 640, 480))
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

    if(R_FAILED(res)) {
        error_display_res(NULL, NULL, NULL, res, "Error capturing camera image.");
    }

    for(int i = 0; i < EVENT_COUNT; i++) {
        if(events[i] != 0) {
            svcCloseHandle(events[i]);
            events[i] = 0;
        }
    }

    svcCloseHandle(data->mutex);
    free(data);
}

Handle task_capture_cam(Handle* mutex, u16* buffer, s16 width, s16 height) {
    if(buffer == NULL || width <= 0 || width > 640 || height <= 0 || height > 480 || mutex == 0) {
        return 0;
    }

    capture_cam_data* data = (capture_cam_data*) calloc(1, sizeof(capture_cam_data));
    if(data == NULL) {
        error_display(NULL, NULL, NULL, "Failed to allocate camera capture data.");

        return 0;
    }

    data->buffer = buffer;
    data->width = width;
    data->height = height;

    Result eventRes = svcCreateEvent(&data->cancelEvent, 1);
    if(R_FAILED(eventRes)) {
        error_display_res(NULL, NULL, NULL, eventRes, "Failed to create camera capture cancel event.");

        free(data);
        return 0;
    }

    Result mutexRes = svcCreateMutex(&data->mutex, false);
    if(R_FAILED(mutexRes)) {
        error_display_res(NULL, NULL, NULL, mutexRes, "Failed to create camera capture buffer mutex.");

        svcCloseHandle(data->cancelEvent);
        free(data);
        return 0;
    }

    if(threadCreate(task_capture_cam_thread, data, 0x10000, 0x19, 1, true) == NULL) {
        error_display(NULL, NULL, NULL, "Failed to create camera capture thread.");

        svcCloseHandle(data->mutex);
        svcCloseHandle(data->cancelEvent);
        free(data);
        return 0;
    }

    *mutex = data->mutex;
    return data->cancelEvent;
}