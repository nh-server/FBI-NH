#include <malloc.h>
#include <string.h>

#include <3ds.h>
#include <errno.h>

#include "../../list.h"
#include "../../error.h"
#include "task.h"

typedef struct {
    move_data_info* info;

    Handle cancelEvent;
} move_data_data;

static bool task_move_data_item(move_data_data* data, u32 index) {
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
                    if(data->info->moveEmpty) {
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
                            if(task_is_quit_all() || svcWaitSynchronization(data->cancelEvent, 0) == 0) {
                                res = MAKERESULT(RL_PERMANENT, RS_CANCELED, RM_APPLICATION, RD_CANCEL_REQUESTED);
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
                        res = MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, RM_APPLICATION, RD_OUT_OF_MEMORY);
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
        if(res == -1) {
            return data->info->ioError(data->info->data, index, errno);
        } else {
            return data->info->resultError(data->info->data, index, res);
        }
    }

    return true;
}

static void task_move_data_thread(void* arg) {
    move_data_data* data = (move_data_data*) arg;

    data->info->finished = false;
    data->info->premature = false;

    data->info->processed = 0;

    for(data->info->processed = 0; data->info->processed < data->info->total; data->info->processed++) {
        if(!task_move_data_item(data, data->info->processed)) {
            data->info->premature = true;
            break;
        }
    }

    data->info->finished = true;

    svcCloseHandle(data->cancelEvent);
    free(data);
}

static void task_move_data_reset_info(move_data_info* info) {
    info->finished = false;
    info->premature = false;

    info->processed = 0;

    info->currProcessed = 0;
    info->currTotal = 0;
}

Handle task_move_data(move_data_info* info) {
    if(info == NULL) {
        return 0;
    }

    task_move_data_reset_info(info);

    move_data_data* installData = (move_data_data*) calloc(1, sizeof(move_data_data));
    installData->info = info;

    Result eventRes = svcCreateEvent(&installData->cancelEvent, 1);
    if(R_FAILED(eventRes)) {
        error_display_res(NULL, NULL, NULL, eventRes, "Failed to create CIA installation cancel event.");

        free(installData);
        return 0;
    }

    if(threadCreate(task_move_data_thread, installData, 0x4000, 0x18, 1, true) == NULL) {
        error_display(NULL, NULL, NULL, "Failed to create CIA installation thread.");

        svcCloseHandle(installData->cancelEvent);
        free(installData);
        return 0;
    }

    return installData->cancelEvent;
}