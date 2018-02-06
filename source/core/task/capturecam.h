#pragma once

typedef enum capture_cam_camera_e {
    CAMERA_OUTER,
    CAMERA_INNER
} capture_cam_camera;

typedef struct capture_cam_data_s {
    u16* buffer;
    s16 width;
    s16 height;
    capture_cam_camera camera;

    Handle mutex;

    volatile bool finished;
    Result result;
    Handle cancelEvent;
} capture_cam_data;

Result task_capture_cam(capture_cam_data* data);