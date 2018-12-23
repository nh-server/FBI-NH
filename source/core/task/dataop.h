#pragma once

typedef struct json_t json_t;

typedef struct ui_view_s ui_view;

#define DOWNLOAD_URL_MAX 1024

typedef enum data_op_e {
    DATAOP_COPY,
    DATAOP_DOWNLOAD,
    DATAOP_DELETE
} data_op;

typedef struct data_op_data_s {
    void* data;

    data_op op;

    u32 processed;
    u32 total;

    // Copy/Download
    u64 currProcessed;
    u64 currTotal;

    u32 bytesPerSecond;
    u32 estimatedRemainingSeconds;

    u32 bufferSize;

    Result (*openDst)(void* data, u32 index, void* initialReadBlock, u64 size, u32* handle);
    Result (*closeDst)(void* data, u32 index, bool succeeded, u32 handle);

    Result (*writeDst)(void* data, u32 handle, u32* bytesWritten, void* buffer, u64 offset, u32 size);

    // Copy
    bool copyEmpty;

    Result (*isSrcDirectory)(void* data, u32 index, bool* isDirectory);
    Result (*makeDstDirectory)(void* data, u32 index);

    Result (*openSrc)(void* data, u32 index, u32* handle);
    Result (*closeSrc)(void* data, u32 index, bool succeeded, u32 handle);

    Result (*getSrcSize)(void* data, u32 handle, u64* size);
    Result (*readSrc)(void* data, u32 handle, u32* bytesRead, void* buffer, u64 offset, u32 size);

    // Download
    Result (*getSrcUrl)(void* data, u32 index, char* url, size_t maxSize);

    // Delete
    Result (*delete)(void* data, u32 index);

    // Suspend
    Result (*suspend)(void* data, u32 index);
    Result (*restore)(void* data, u32 index);

    // Errors
    bool (*error)(void* data, u32 index, Result res, ui_view** errorView);

    // General
    volatile bool finished;
    Result result;
    Handle cancelEvent;

    // Internal
    volatile bool retryResponse;
} data_op_data;

Result task_data_op(data_op_data* data);