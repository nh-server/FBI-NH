#include <malloc.h>
#include <string.h>

#include <3ds.h>
#include <curl/curl.h>
#include <jansson.h>

#include "dataop.h"
#include "../core.h"

static Result task_data_op_check_running(data_op_data* data, u32 index, u32* srcHandle, u32* dstHandle) {
    Result res = 0;

    if(task_is_quit_all() || svcWaitSynchronization(data->cancelEvent, 0) == 0) {
        res = R_APP_CANCELLED;
    } else {
        bool suspended = svcWaitSynchronization(task_get_suspend_event(), 0) != 0;
        if(suspended) {
            if(data->op == DATAOP_COPY && srcHandle != NULL && dstHandle != NULL && data->suspendTransfer != NULL && R_SUCCEEDED(res)) {
                res = data->suspendTransfer(data->data, index, srcHandle, dstHandle);
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

            if(data->op == DATAOP_COPY && srcHandle != NULL && dstHandle != NULL && data->restoreTransfer != NULL && R_SUCCEEDED(res)) {
                res = data->restoreTransfer(data->data, index, srcHandle, dstHandle);
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
                            if(R_FAILED(res = task_data_op_check_running(data, data->processed, &srcHandle, &dstHandle))) {
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

static Result task_download_execute(const char* url, void* data, size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata),
                                                                 int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)) {
    Result res = 0;

    CURL* curl = curl_easy_init();
    if(curl != NULL) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, data);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, HTTP_USER_AGENT);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, true);
        curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, HTTP_CONNECT_TIMEOUT);

        if(progress_callback != NULL) {
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, data);
        }

        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false); // TODO: Certificates?

        CURLcode ret = curl_easy_perform(curl);
        if(ret == CURLE_OK) {
            long responseCode;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

            if(responseCode >= 400) {
                return R_APP_HTTP_ERROR_BASE + ret;
            }
        } else {
            res = R_APP_CURL_ERROR_BASE + ret;
        }

        curl_easy_cleanup(curl);
    } else {
        res = R_APP_CURL_INIT_FAILED;
    }

    return res;
}

typedef struct {
    u8* buf;
    u32 size;

    u32 pos;
} download_sync_data;

static size_t task_download_sync_write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    download_sync_data* data = (download_sync_data*) userdata;

    size_t realSize = size * nmemb;
    size_t remaining = data->size - data->pos;
    size_t copy = realSize < remaining ? realSize : remaining;

    memcpy(&data->buf[data->pos], ptr, copy);
    data->pos += copy;

    return copy;
}

Result task_download_sync(const char* url, u32* downloadedSize, void* buf, size_t size) {
#ifdef USE_CURL
    if(url == NULL || buf == NULL) {
        return R_APP_INVALID_ARGUMENT;
    }

    Result res = 0;

    download_sync_data readData = {buf, size, 0};
    if(R_SUCCEEDED(res = task_download_execute(url, &readData, task_download_sync_write_callback, NULL))) {
        if(downloadedSize != NULL) {
            *downloadedSize = readData.pos;
        }
    }

    return res;
#else
    Result res = 0;

    http_context context = NULL;
    if(R_SUCCEEDED(res = http_open(&context, url, true))) {
        res = http_read(context, downloadedSize, buf, size);

        Result closeRes = http_close(context);
        if(R_SUCCEEDED(res)) {
            res = closeRes;
        }
    }

    return res;
#endif
}

Result task_download_json_sync(const char* url, json_t** json, size_t maxSize) {
    if(url == NULL || json == NULL) {
        return R_APP_INVALID_ARGUMENT;
    }

    Result res = 0;

    char* text = (char*) calloc(sizeof(char), maxSize);
    if(text != NULL) {
        u32 textSize = 0;
        if(R_SUCCEEDED(res = task_download_sync(url, &textSize, text, maxSize))) {
            json_error_t error;
            json_t* parsed = json_loads(text, 0, &error);
            if(parsed != NULL) {
                *json = parsed;
            } else {
                res = R_APP_PARSE_FAILED;
            }
        }

        free(text);
    } else {
        res = R_APP_OUT_OF_MEMORY;
    }

    return res;
}

static Result FSUSER_AddSeed(u64 titleId, const void* seed) {
    u32 *cmdbuf = getThreadCommandBuffer();

    cmdbuf[0] = 0x087A0180;
    cmdbuf[1] = (u32) (titleId & 0xFFFFFFFF);
    cmdbuf[2] = (u32) (titleId >> 32);
    memcpy(&cmdbuf[3], seed, 16);

    Result ret = 0;
    if(R_FAILED(ret = svcSendSyncRequest(*fsGetSessionHandle()))) return ret;

    ret = cmdbuf[1];
    return ret;
}

Result task_download_seed_sync(u64 titleId) {
    char pathBuf[64];
    snprintf(pathBuf, 64, "/fbi/seed/%016llX.dat", titleId);

    Result res = 0;

    FS_Path* fsPath = fs_make_path_utf8(pathBuf);
    if(fsPath != NULL) {
        u8 seed[16];

        Handle fileHandle = 0;
        if(R_SUCCEEDED(res = FSUSER_OpenFileDirectly(&fileHandle, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), *fsPath, FS_OPEN_READ, 0))) {
            u32 bytesRead = 0;
            res = FSFILE_Read(fileHandle, &bytesRead, 0, seed, sizeof(seed));

            FSFILE_Close(fileHandle);
        }

        fs_free_path_utf8(fsPath);

        if(R_FAILED(res)) {
            u8 region = CFG_REGION_USA;
            CFGU_SecureInfoGetRegion(&region);

            if(region <= CFG_REGION_TWN) {
                static const char* regionStrings[] = {"JP", "US", "GB", "GB", "HK", "KR", "TW"};

                char url[128];
                snprintf(url, 128, "https://kagiya-ctr.cdn.nintendo.net/title/0x%016llX/ext_key?country=%s", titleId, regionStrings[region]);

                u32 downloadedSize = 0;
                if(R_SUCCEEDED(res = task_download_sync(url, &downloadedSize, seed, sizeof(seed))) && downloadedSize != sizeof(seed)) {
                    res = R_APP_BAD_DATA;
                }
            } else {
                res = R_APP_OUT_OF_RANGE;
            }
        }

        if(R_SUCCEEDED(res)) {
            res = FSUSER_AddSeed(titleId, seed);
        }
    } else {
        res = R_APP_OUT_OF_MEMORY;
    }

    return res;
}

typedef struct {
    data_op_data* baseData;
    u32 index;

    u8* buffer;
    u32 bufferPos;
    u32 bufferSize;

    u32 dstHandle;

    u32 bytesWritten;
    u64 ioStartTime;
    u64 lastBytesPerSecondUpdate;
    u32 bytesSinceUpdate;

    Result res;
} data_op_download_data;

static bool task_data_op_download_flush(data_op_download_data* data) {
    if(data->dstHandle == 0 && R_FAILED(data->res = data->baseData->openDst(data->baseData->data, data->index, data->buffer, data->baseData->currTotal, &data->dstHandle))) {
        return false;
    }

    u32 bytesWritten = 0;
    if(R_FAILED(data->res = data->baseData->writeDst(data->baseData->data, data->dstHandle, &bytesWritten, data->buffer, data->bytesWritten, data->bufferPos))) {
        return false;
    }

    data->bytesWritten += data->bufferPos;
    data->bufferPos = 0;

    return true;
}

static size_t task_data_op_download_write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    data_op_download_data* data = (data_op_download_data*) userdata;

    size_t remaining = size * nmemb;
    while(remaining > 0) {
        // Buffering is done to provide adequate data to openDst and prevent misaligned size errors from AM.
        if(data->bufferPos < data->bufferSize) {
            size_t bufferRemaining = data->bufferSize - data->bufferPos;
            size_t used = remaining < bufferRemaining ? remaining : bufferRemaining;

            memcpy(&data->buffer[data->bufferPos], ptr, used);

            data->bufferPos += used;
            remaining -= used;
        }

        if(data->bufferPos >= data->bufferSize) {
            // TODO: Pause on suspend/Unpause on restore?
            u32 srcHandle = 0;
            if(R_FAILED(data->res = task_data_op_check_running(data->baseData, data->baseData->processed, &srcHandle, &data->dstHandle))) {
                return 0;
            }

            if(!task_data_op_download_flush(data)) {
                break;
            }
        }
    }

    return (size * nmemb) - remaining;
}

int task_data_op_download_progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    data_op_download_data* data = (data_op_download_data*) clientp;

    data->bytesSinceUpdate += (u64) dlnow - data->baseData->currProcessed;
    data->baseData->currTotal = (u64) dltotal;
    data->baseData->currProcessed = (u64) dlnow;

    u64 time = osGetTime();
    if(data->lastBytesPerSecondUpdate != 0) {
        u64 elapsed = time - data->lastBytesPerSecondUpdate;
        if(elapsed >= 1000) {
            data->baseData->bytesPerSecond = (u32) (data->bytesSinceUpdate / (elapsed / 1000.0f));

            if(data->ioStartTime != 0) {
                data->baseData->estimatedRemainingSeconds = (u32) ((data->baseData->currTotal - data->baseData->currProcessed) / (data->baseData->currProcessed / ((time - data->ioStartTime) / 1000.0f)));
            } else {
                data->baseData->estimatedRemainingSeconds = 0;
            }

            if(data->ioStartTime == 0 && data->baseData->currProcessed > 0) {
                data->ioStartTime = time;
            }

            data->bytesSinceUpdate = 0;
            data->lastBytesPerSecondUpdate = time;
        }
    } else {
        data->lastBytesPerSecondUpdate = time;
    }

    return 0;
}

static Result task_data_op_download(data_op_data* data, u32 index) {
    data->currProcessed = 0;
    data->currTotal = 0;

    data->bytesPerSecond = 0;
    data->estimatedRemainingSeconds = 0;

    Result res = 0;

    void* buffer = calloc(1, data->bufferSize);
    if(buffer != NULL) {
        data_op_download_data downloadData = {data, index, buffer, 0, data->bufferSize, 0, 0, 0, 0, 0, 0};
        res = task_download_execute(data->downloadUrls[index], &downloadData, task_data_op_download_write_callback, task_data_op_download_progress_callback);

        if(downloadData.res != 0) {
            res = downloadData.res;
        }

        if(R_SUCCEEDED(res) && downloadData.bufferPos > 0) {
            task_data_op_download_flush(&downloadData);
        }

        if(downloadData.dstHandle != 0) {
            Result closeDstRes = data->closeDst(data->data, index, res == 0, downloadData.dstHandle);
            if(R_SUCCEEDED(res)) {
                res = closeDstRes;
            }
        }

        free(buffer);
    } else {
        res = R_APP_OUT_OF_MEMORY;
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

        if(R_SUCCEEDED(res = task_data_op_check_running(data, data->processed, NULL, NULL))) {
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