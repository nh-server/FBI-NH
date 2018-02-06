#pragma once

#define CAMERA_QR_DATA_MAX 8896

typedef enum capture_cam_camera_e {
    CAMERA_OUTER,
    CAMERA_INNER
} capture_cam_camera;

typedef struct capture_cam_data_s {
    u16* buffer;
    s16 width;
    s16 height;
    capture_cam_camera camera;
    bool scanQR;

    bool qrReady;
    u8 qrData[CAMERA_QR_DATA_MAX];

    Handle mutex;

    volatile bool finished;
    Result result;
    Handle cancelEvent;
} capture_cam_data;

Result task_capture_cam(capture_cam_data* data);