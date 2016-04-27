#include <malloc.h>
#include <string.h>

#include <3ds.h>

#include "task.h"
#include "../../list.h"
#include "../../error.h"

typedef struct {
    data_op_info* info;

    Handle cancelEvent;
} data_op_data;

static bool task_data_op_copy(data_op_data* data, u32 index) {
    data->info->currProcessed = 0;
    data->info->currTotal = 0;

    Result res = 0;

    bool isDir = false;
    if(R_SUCCEEDED(res = data->info->isSrcDirectory(data->info->data, index, &isDir)) && isDir) {
        res = data->info->makeDstDirectory(data->info->data, index);
    } else {
        u32 srcHandle = 0;
        if(R_SUCCEEDED(res = data->info->openSrc(data->info->data, index, &srcHandle))) {
            if(R_SUCCEEDED(res = data->info->getSrcSize(data->info->data, srcHandle, &data->info->currTotal))) {
                if(data->info->currTotal == 0) {
                    if(data->info->copyEmpty) {
                        u32 dstHandle = 0;
                        if(R_SUCCEEDED(res = data->info->openDst(data->info->data, index, NULL, &dstHandle))) {
                            res = data->info->closeDst(data->info->data, index, true, dstHandle);
                        }
                    }
                } else {
                    u32 bufferSize = 1024 * 256;
                    u8* buffer = (u8*) calloc(1, bufferSize);
                    if(buffer != NULL) {
                        u32 dstHandle = 0;

                        bool firstRun = true;
                        while(data->info->currProcessed < data->info->currTotal) {
                            svcWaitSynchronization(task_get_pause_event(), U64_MAX);
                            if(task_is_quit_all() || svcWaitSynchronization(data->cancelEvent, 0) == 0) {
                                res = R_FBI_CANCELLED;
                                break;
                            }

                            u32 currSize = bufferSize;
                            if((u64) currSize > data->info->currTotal - data->info->currProcessed) {
                                currSize = (u32) (data->info->currTotal - data->info->currProcessed);
                            }

                            u32 bytesRead = 0;
                            u32 bytesWritten = 0;
                            if(R_FAILED(res = data->info->readSrc(data->info->data, srcHandle, &bytesRead, buffer, data->info->currProcessed, currSize))) {
                                break;
                            }

                            if(firstRun) {
                                firstRun = false;

                                if(R_FAILED(res = data->info->openDst(data->info->data, index, buffer, &dstHandle))) {
                                    break;
                                }
                            }

                            if(R_FAILED(res = data->info->writeDst(data->info->data, dstHandle, &bytesWritten, buffer, data->info->currProcessed, currSize))) {
                                break;
                            }

                            data->info->currProcessed += bytesWritten;
                        }

                        if(dstHandle != 0) {
                            Result closeDstRes = data->info->closeDst(data->info->data, index, res == 0, dstHandle);
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

            Result closeSrcRes = data->info->closeSrc(data->info->data, index, res == 0, srcHandle);
            if(R_SUCCEEDED(res)) {
                res = closeSrcRes;
            }
        }
    }

    if(R_FAILED(res)) {
        return data->info->error(data->info->data, index, res);
    }

    return true;
}

static bool task_data_op_delete(data_op_data* data, u32 index) {
    Result res = 0;
    if(R_FAILED(res = data->info->delete(data->info->data, index))) {
        return data->info->error(data->info->data, index, res);
    }

    return true;
}

static void task_data_op_thread(void* arg) {
    data_op_data* data = (data_op_data*) arg;

    data->info->finished = false;
    data->info->premature = false;

    data->info->processed = 0;

    for(data->info->processed = 0; data->info->processed < data->info->total; data->info->processed++) {
        bool cont = false;

        switch(data->info->op) {
            case DATAOP_COPY:
                cont = task_data_op_copy(data, data->info->processed);
                break;
            case DATAOP_DELETE:
                cont = task_data_op_delete(data, data->info->processed);
                break;
            default:
                break;
        }

        if(!cont) {
            data->info->premature = true;
            break;
        }
    }

    data->info->finished = true;

    svcCloseHandle(data->cancelEvent);
    free(data);
}

static void task_data_op_reset_info(data_op_info* info) {
    info->finished = false;
    info->premature = false;

    info->processed = 0;

    info->currProcessed = 0;
    info->currTotal = 0;
}

Handle task_data_op(data_op_info* info) {
    if(info == NULL) {
        return 0;
    }

    task_data_op_reset_info(info);

    data_op_data* data = (data_op_data*) calloc(1, sizeof(data_op_data));
    if(data == NULL) {
        error_display(NULL, NULL, NULL, "Failed to allocate data operation data.");

        return 0;
    }

    data->info = info;

    Result eventRes = svcCreateEvent(&data->cancelEvent, 1);
    if(R_FAILED(eventRes)) {
        error_display_res(NULL, NULL, NULL, eventRes, "Failed to create data operation cancel event.");

        free(data);
        return 0;
    }

    if(threadCreate(task_data_op_thread, data, 0x10000, 0x18, 1, true) == NULL) {
        error_display(NULL, NULL, NULL, "Failed to create data operation thread.");

        svcCloseHandle(data->cancelEvent);
        free(data);
        return 0;
    }

    return data->cancelEvent;
}