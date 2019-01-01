#include <malloc.h>
#include <string.h>

#include <3ds.h>
#include <jansson.h>

#include "dataop.h"
#include "../core.h"

static Result task_data_op_check_running(data_op_data* data) {
    Result res = 0;

    if(task_is_quit_all() || svcWaitSynchronization(data->cancelEvent, 0) == 0) {
        res = R_APP_CANCELLED;
    } else {
        bool suspended = svcWaitSynchronization(task_get_suspend_event(), 0) != 0;
        if(suspended) {
            if(data->suspend != NULL && R_SUCCEEDED(res)) {
                res = data->suspend(data->data, data->processed);
            }
        }

        svcWaitSynchronization(task_get_pause_event(), U64_MAX);

        if(suspended) {
            if(data->restore != NULL && R_SUCCEEDED(res)) {
                res = data->restore(data->data, data->processed);
            }
        }
    }

    return res;
}

static Result task_data_op_copy(data_op_data* data, u32 index) {
    data->currProcessed = 0;
    data->currTotal = 0;

    data->bytesPerSecond = 0;
    data->estimatedRemainingSeconds = 0;

    Result res = 0;

    bool isDir = false;
    if(R_SUCCEEDED(res = data->isSrcDirectory(data->data, index, &isDir)) && isDir) {
        res = data->makeDstDirectory(data->data, index);
    } else {
        u32 srcHandle = 0;
        if(R_SUCCEEDED(res = data->openSrc(data->data, index, &srcHandle))) {
            if(R_SUCCEEDED(res = data->getSrcSize(data->data, srcHandle, &data->currTotal))) {
                if(data->currTotal == 0) {
                    if(data->copyEmpty) {
                        u32 dstHandle = 0;
                        if(R_SUCCEEDED(res = data->openDst(data->data, index, NULL, data->currTotal, &dstHandle))) {
                            res = data->closeDst(data->data, index, true, dstHandle);
                        }
                    } else {
                        res = R_APP_BAD_DATA;
                    }
                } else {
                    u8* buffer = (u8*) calloc(1, data->bufferSize);
                    if(buffer != NULL) {
                        u32 dstHandle = 0;

                        u64 ioStartTime = 0;
                        u64 lastBytesPerSecondUpdate = osGetTime();
                        u32 bytesSinceUpdate = 0;

                        bool firstRun = true;
                        while(data->currProcessed < data->currTotal) {
                            if(R_FAILED(res = task_data_op_check_running(data))) {
                                break;
                            }

                            u32 bytesRead = 0;
                            if(R_FAILED(res = data->readSrc(data->data, srcHandle, &bytesRead, buffer, data->currProcessed, data->bufferSize))) {
                                break;
                            }

                            if(firstRun) {
                                firstRun = false;

                                if(R_FAILED(res = data->openDst(data->data, index, buffer, data->currTotal, &dstHandle))) {
                                    break;
                                }
                            }

                            u32 bytesWritten = 0;
                            if(R_FAILED(res = data->writeDst(data->data, dstHandle, &bytesWritten, buffer, data->currProcessed, bytesRead))) {
                                break;
                            }

                            data->currProcessed += bytesWritten;
                            bytesSinceUpdate += bytesWritten;

                            u64 time = osGetTime();
                            u64 elapsed = time - lastBytesPerSecondUpdate;
                            if(elapsed >= 1000) {
                                data->bytesPerSecond = (u32) (bytesSinceUpdate / (elapsed / 1000.0f));

                                if(ioStartTime != 0) {
                                    data->estimatedRemainingSeconds = (u32) ((data->currTotal - data->currProcessed) / (data->currProcessed / ((time - ioStartTime) / 1000.0f)));
                                } else {
                                    data->estimatedRemainingSeconds = 0;
                                }

                                if(ioStartTime == 0 && data->currProcessed > 0) {
                                    ioStartTime = time;
                                }

                                bytesSinceUpdate = 0;
                                lastBytesPerSecondUpdate = time;
                            }
                        }

                        if(dstHandle != 0) {
                            Result closeDstRes = data->closeDst(data->data, index, res == 0, dstHandle);
                            if(R_SUCCEEDED(res)) {
                                res = closeDstRes;
                            }
                        }

                        free(buffer);
                    } else {
                        res = R_APP_OUT_OF_MEMORY;
                    }
                }
            }

            Result closeSrcRes = data->closeSrc(data->data, index, res == 0, srcHandle);
            if(R_SUCCEEDED(res)) {
                res = closeSrcRes;
            }
        }
    }

    return res;
}

typedef struct {
    data_op_data* data;

    u32 index;

    u32 dstHandle;
    bool firstRun;
    u64 ioStartTime;
    u64 lastBytesPerSecondUpdate;
    u32 bytesSinceUpdate;

    u64 writeOffset;
} data_op_download_data;

static Result task_data_op_download_callback(void* userData, void* buffer, size_t size) {
    data_op_download_data* downloadData = (data_op_download_data*) userData;
    data_op_data* data = downloadData->data;

    if(downloadData->firstRun) {
        downloadData->firstRun = false;

        Result res = data->openDst(data->data, downloadData->index, buffer, data->currTotal, &downloadData->dstHandle);
        if(R_FAILED(res)) {
            return res;
        }
    }

    u32 bytesWritten = 0;
    Result res = data->writeDst(data->data, downloadData->dstHandle, &bytesWritten, buffer, downloadData->writeOffset, size);
    downloadData->writeOffset += bytesWritten;

    return res;
}

static Result task_data_op_download_check_running(void* userData) {
    data_op_download_data* downloadData = (data_op_download_data*) userData;

    return task_data_op_check_running(downloadData->data);
}

static Result task_data_op_download_progress(void* userData, u64 total, u64 curr) {
    data_op_download_data* downloadData = (data_op_download_data*) userData;
    data_op_data* data = downloadData->data;

    downloadData->bytesSinceUpdate += curr - data->currProcessed;

    data->currTotal = total;
    data->currProcessed = curr;

    u64 time = osGetTime();
    u64 elapsed = time - downloadData->lastBytesPerSecondUpdate;
    if(elapsed >= 1000) {
        data->bytesPerSecond = (u32) (downloadData->bytesSinceUpdate / (elapsed / 1000.0f));

        if(downloadData->ioStartTime != 0) {
            data->estimatedRemainingSeconds = (u32) ((data->currTotal - data->currProcessed) / (data->currProcessed / ((time - downloadData->ioStartTime) / 1000.0f)));
        } else {
            data->estimatedRemainingSeconds = 0;
        }

        if(downloadData->ioStartTime == 0 && data->currProcessed > 0) {
            downloadData->ioStartTime = time;
        }

        downloadData->bytesSinceUpdate = 0;
        downloadData->lastBytesPerSecondUpdate = time;
    }

    return 0;
}

static Result task_data_op_download(data_op_data* data, u32 index) {
    data->currProcessed = 0;
    data->currTotal = 0;

    data->bytesPerSecond = 0;
    data->estimatedRemainingSeconds = 0;

    Result res = 0;

    char url[DOWNLOAD_URL_MAX];
    if(R_SUCCEEDED(res = data->getSrcUrl(data->data, index, url, DOWNLOAD_URL_MAX))) {
        data_op_download_data downloadData = {data, index, 0, true, 0, osGetTime(), 0, 0};
        res = http_download_callback(url, data->bufferSize, &downloadData, task_data_op_download_callback, task_data_op_download_check_running, task_data_op_download_progress);

        if(downloadData.dstHandle != 0) {
            Result closeDstRes = data->closeDst(data->data, index, res == 0, downloadData.dstHandle);
            if(R_SUCCEEDED(res)) {
                res = closeDstRes;
            }
        }
    }

    return res;
}

static Result task_data_op_delete(data_op_data* data, u32 index) {
    return data->delete(data->data, index);
}

static void task_data_op_retry_onresponse(ui_view* view, void* data, u32 response) {
    ((data_op_data*) data)->retryResponse = response == PROMPT_YES;
}

static void task_data_op_thread(void* arg) {
    data_op_data* data = (data_op_data*) arg;

    for(data->processed = 0; data->processed < data->total; data->processed++) {
        Result res = 0;

        if(R_SUCCEEDED(res = task_data_op_check_running(data))) {
            switch(data->op) {
                case DATAOP_COPY:
                    res = task_data_op_copy(data, data->processed);
                    break;
                case DATAOP_DOWNLOAD:
                    res = task_data_op_download(data, data->processed);
                    break;
                case DATAOP_DELETE:
                    res = task_data_op_delete(data, data->processed);
                    break;
                default:
                    break;
            }
        }

        data->result = res;

        if(R_FAILED(res)) {
            if(res == R_APP_CANCELLED) {
                prompt_display_notify("Failure", "Operation cancelled.", COLOR_TEXT, NULL, NULL, NULL);
                break;
            } else if(res != R_APP_SKIPPED) {
                ui_view* errorView = NULL;
                bool proceed = data->error(data->data, data->processed, res, &errorView);

                if(errorView != NULL) {
                    svcWaitSynchronization(errorView->active, U64_MAX);
                }

                ui_view* retryView = prompt_display_yes_no("Confirmation", "Retry?", COLOR_TEXT, data, NULL, task_data_op_retry_onresponse);
                if(retryView != NULL) {
                    svcWaitSynchronization(retryView->active, U64_MAX);

                    if(data->retryResponse) {
                        if(proceed) {
                            data->processed--;
                        } else {
                            data->processed = 0;
                        }
                    } else if(!proceed) {
                        break;
                    }
                }
            }
        }
    }

    svcCloseHandle(data->cancelEvent);

    data->finished = true;

    aptSetSleepAllowed(true);
}

Result task_data_op(data_op_data* data) {
    if(data == NULL) {
        return R_APP_INVALID_ARGUMENT;
    }

    data->processed = 0;

    data->currProcessed = 0;
    data->currTotal = 0;

    data->finished = false;
    data->result = 0;
    data->cancelEvent = 0;

    Result res = 0;
    if(R_SUCCEEDED(res = svcCreateEvent(&data->cancelEvent, RESET_STICKY))) {
        if(threadCreate(task_data_op_thread, data, 0x10000, 0x18, 1, true) == NULL) {
            res = R_APP_THREAD_CREATE_FAILED;
        }
    }

    if(R_FAILED(res)) {
        data->finished = true;

        if(data->cancelEvent != 0) {
            svcCloseHandle(data->cancelEvent);
            data->cancelEvent = 0;
        }
    }

    aptSetSleepAllowed(false);

    return res;
}