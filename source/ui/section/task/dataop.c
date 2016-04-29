#include <malloc.h>
#include <string.h>

#include <3ds.h>

#include "task.h"
#include "../../list.h"
#include "../../error.h"

static bool task_data_op_copy(data_op_data* data, u32 index) {
    data->currProcessed = 0;
    data->currTotal = 0;

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
                        if(R_SUCCEEDED(res = data->openDst(data->data, index, NULL, &dstHandle))) {
                            res = data->closeDst(data->data, index, true, dstHandle);
                        }
                    }
                } else {
                    u32 bufferSize = 1024 * 256;
                    u8* buffer = (u8*) calloc(1, bufferSize);
                    if(buffer != NULL) {
                        u32 dstHandle = 0;

                        bool firstRun = true;
                        while(data->currProcessed < data->currTotal) {
                            svcWaitSynchronization(task_get_pause_event(), U64_MAX);
                            if(task_is_quit_all() || svcWaitSynchronization(data->cancelEvent, 0) == 0) {
                                res = R_FBI_CANCELLED;
                                break;
                            }

                            u32 currSize = bufferSize;
                            if((u64) currSize > data->currTotal - data->currProcessed) {
                                currSize = (u32) (data->currTotal - data->currProcessed);
                            }

                            u32 bytesRead = 0;
                            u32 bytesWritten = 0;
                            if(R_FAILED(res = data->readSrc(data->data, srcHandle, &bytesRead, buffer, data->currProcessed, currSize))) {
                                break;
                            }

                            if(firstRun) {
                                firstRun = false;

                                if(R_FAILED(res = data->openDst(data->data, index, buffer, &dstHandle))) {
                                    break;
                                }
                            }

                            if(R_FAILED(res = data->writeDst(data->data, dstHandle, &bytesWritten, buffer, data->currProcessed, currSize))) {
                                break;
                            }

                            data->currProcessed += bytesWritten;
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

    if(R_FAILED(res)) {
        data->result = res;
        return data->error(data->data, index, res);
    }

    return true;
}

static bool task_data_op_delete(data_op_data* data, u32 index) {
    Result res = 0;
    if(R_FAILED(res = data->delete(data->data, index))) {
        return data->error(data->data, index, res);
    }

    return true;
}

static void task_data_op_thread(void* arg) {
    data_op_data* data = (data_op_data*) arg;

    for(data->processed = 0; data->processed < data->total; data->processed++) {
        bool cont = false;

        switch(data->op) {
            case DATAOP_COPY:
                cont = task_data_op_copy(data, data->processed);
                break;
            case DATAOP_DELETE:
                cont = task_data_op_delete(data, data->processed);
                break;
            default:
                break;
        }

        if(!cont) {
            break;
        }
    }

    svcCloseHandle(data->cancelEvent);

    data->finished = true;
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
    if(R_SUCCEEDED(res = svcCreateEvent(&data->cancelEvent, 1))) {
        if(threadCreate(task_data_op_thread, data, 0x10000, 0x18, 1, true) == NULL) {
            res = R_FBI_THREAD_CREATE_FAILED;
        }
    }

    if(R_FAILED(res)) {
        if(data->cancelEvent != 0) {
            svcCloseHandle(data->cancelEvent);
            data->cancelEvent = 0;
        }
    }

    return res;
}