#include <malloc.h>
#include <string.h>

#include <3ds.h>

#include "task.h"
#include "../../error.h"

static Result task_data_op_check_running(data_op_data* data, u32 index, u32* srcHandle, u32* dstHandle) {
    Result res = 0;

    if(task_is_quit_all() || svcWaitSynchronization(data->cancelEvent, 0) == 0) {
        res = R_FBI_CANCELLED;
    } else {
        bool suspended = svcWaitSynchronization(task_get_suspend_event(), 0) != 0;
        if(suspended) {
            if(data->op == DATAOP_COPY && srcHandle != NULL && dstHandle != NULL && data->suspendCopy != NULL && R_SUCCEEDED(res)) {
                res = data->suspendCopy(data->data, index, srcHandle, dstHandle);
            }

            if(data->suspend != NULL && R_SUCCEEDED(res)) {
                res = data->suspend(data->data, index);
            }
        }

        svcWaitSynchronization(task_get_pause_event(), U64_MAX);

        if(suspended) {
            if(data->restore != NULL && R_SUCCEEDED(res)) {
                res = data->restore(data->data, index);
            }

            if(data->op == DATAOP_COPY && srcHandle != NULL && dstHandle != NULL && data->restoreCopy != NULL && R_SUCCEEDED(res)) {
                res = data->restoreCopy(data->data, index, srcHandle, dstHandle);
            }
        }
    }

    return res;
}

static Result task_data_op_copy(data_op_data* data, u32 index) {
    data->currProcessed = 0;
    data->currTotal = 0;

    data->copyBytesPerSecond = 0;

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
                        res = R_FBI_BAD_DATA;
                    }
                } else {
                    u8* buffer = (u8*) calloc(1, data->copyBufferSize);
                    if(buffer != NULL) {
                        u32 dstHandle = 0;

                        u64 ioStartTime = 0;
                        u64 lastBytesPerSecondUpdate = osGetTime();
                        u32 bytesSinceUpdate = 0;

                        bool firstRun = true;
                        while(data->currProcessed < data->currTotal) {
                            if(R_FAILED(res = task_data_op_check_running(data, data->processed, &srcHandle, &dstHandle))) {
                                break;
                            }

                            u32 bytesRead = 0;
                            if(R_FAILED(res = data->readSrc(data->data, srcHandle, &bytesRead, buffer, data->currProcessed, data->copyBufferSize))) {
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
                                data->copyBytesPerSecond = (u32) (bytesSinceUpdate / (elapsed / 1000.0f));

                                if (ioStartTime != 0) {
                                        data->estimatedRemainingSeconds = (u32) ((data->currTotal - data->currProcessed) / (data->currProcessed / ((time - ioStartTime) / 1000.0f)));
                                } else {
                                        data->estimatedRemainingSeconds = 0;
                                }
                                if (ioStartTime == 0 && data->currProcessed > 0) {
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
                        res = R_FBI_OUT_OF_MEMORY;
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

static Result task_data_op_delete(data_op_data* data, u32 index) {
    return data->delete(data->data, index);
}

static void task_data_op_thread(void* arg) {
    data_op_data* data = (data_op_data*) arg;

    for(data->processed = 0; data->processed < data->total; data->processed++) {
        Result res = 0;

        if(R_SUCCEEDED(res = task_data_op_check_running(data, data->processed, NULL, NULL))) {
            switch(data->op) {
                case DATAOP_COPY:
                    res = task_data_op_copy(data, data->processed);
                    break;
                case DATAOP_DELETE:
                    res = task_data_op_delete(data, data->processed);
                    break;
                default:
                    break;
            }
        }

        data->result = res;

        if(R_FAILED(res) && !data->error(data->data, data->processed, res)) {
            break;
        }
    }

    svcCloseHandle(data->cancelEvent);

    data->finished = true;

    aptSetSleepAllowed(true);
}

Result task_data_op(data_op_data* data) {
    if(data == NULL) {
        return R_FBI_INVALID_ARGUMENT;
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
            res = R_FBI_THREAD_CREATE_FAILED;
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