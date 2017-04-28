#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <3ds.h>

#include "section.h"
#include "task/task.h"
#include "../error.h"
#include "../info.h"
#include "../prompt.h"
#include "../ui.h"
#include "../../core/screen.h"
#include "../../core/util.h"
#include "../../json/json.h"

#define URL_MAX 1024

typedef struct {
    char url[URL_MAX];

    u32 responseCode;
    data_op_data installInfo;
} update_data;

static Result update_is_src_directory(void* data, u32 index, bool* isDirectory) {
    *isDirectory = false;
    return 0;
}

static Result update_make_dst_directory(void* data, u32 index) {
    return 0;
}

static Result update_open_src(void* data, u32 index, u32* handle) {
    update_data* updateData = (update_data*) data;

    Result res = 0;

    httpcContext* context = (httpcContext*) calloc(1, sizeof(httpcContext));
    if(context != NULL) {
        if(R_SUCCEEDED(res = util_http_open(context, &updateData->responseCode, updateData->url, true))) {
            *handle = (u32) context;
        } else {
            free(context);
        }
    } else {
        res = R_FBI_OUT_OF_MEMORY;
    }

    return res;
}

static Result update_close_src(void* data, u32 index, bool succeeded, u32 handle) {
    return util_http_close((httpcContext*) handle);
}

static Result update_get_src_size(void* data, u32 handle, u64* size) {
    u32 downloadSize = 0;
    Result res = util_http_get_size((httpcContext*) handle, &downloadSize);

    *size = downloadSize;
    return res;
}

static Result update_read_src(void* data, u32 handle, u32* bytesRead, void* buffer, u64 offset, u32 size) {
    return util_http_read((httpcContext*) handle, bytesRead, buffer, size);
}

static Result update_open_dst(void* data, u32 index, void* initialReadBlock, u64 size, u32* handle) {
    if(util_get_3dsx_path() != NULL) {
        FS_Path* path = util_make_path_utf8(util_get_3dsx_path());
        if(path != NULL) {
            Result res = FSUSER_OpenFileDirectly(handle, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), *path, FS_OPEN_WRITE | FS_OPEN_CREATE, 0);

            util_free_path_utf8(path);
            return res;
        } else {
            return R_FBI_OUT_OF_MEMORY;
        }
    } else {
        return AM_StartCiaInstall(MEDIATYPE_SD, handle);
    }

}

static Result update_close_dst(void* data, u32 index, bool succeeded, u32 handle) {
    if(util_get_3dsx_path() != NULL) {
        return FSFILE_Close(handle);
    } else {
        if(succeeded) {
            return AM_FinishCiaInstall(handle);
        } else {
            return AM_CancelCIAInstall(handle);
        }
    }
}

static Result update_write_dst(void* data, u32 handle, u32* bytesWritten, void* buffer, u64 offset, u32 size) {
    return FSFILE_Write(handle, bytesWritten, offset, buffer, size, 0);
}

static Result update_suspend_copy(void* data, u32 index, u32* srcHandle, u32* dstHandle) {
    return 0;
}

static Result update_restore_copy(void* data, u32 index, u32* srcHandle, u32* dstHandle) {
    return 0;
}

static Result update_suspend(void* data, u32 index) {
    return 0;
}

static Result update_restore(void* data, u32 index) {
    return 0;
}

static bool update_error(void* data, u32 index, Result res) {
    update_data* updateData = (update_data*) data;

    if(res == R_FBI_CANCELLED) {
        prompt_display_notify("Failure", "Install cancelled.", COLOR_TEXT, NULL, NULL, NULL);
    } else if(res == R_FBI_HTTP_RESPONSE_CODE) {
        error_display(NULL, NULL, "Failed to update FBI.\nHTTP server returned response code %d", updateData->responseCode);
    } else {
        error_display_res(NULL, NULL, res, "Failed to update FBI.");
    }

    return false;
}

static void update_install_update(ui_view* view, void* data, float* progress, char* text) {
    update_data* updateData = (update_data*) data;

    if(updateData->installInfo.finished) {
        ui_pop();
        info_destroy(view);

        if(R_SUCCEEDED(updateData->installInfo.result)) {
            prompt_display_notify("Success", "Update complete.", COLOR_TEXT, NULL, NULL, NULL);
        }

        free(updateData);

        return;
    }

    if(hidKeysDown() & KEY_B) {
        svcSignalEvent(updateData->installInfo.cancelEvent);
    }

    *progress = updateData->installInfo.currTotal != 0 ? (float) ((double) updateData->installInfo.currProcessed / (double) updateData->installInfo.currTotal) : 0;
    snprintf(text, PROGRESS_TEXT_MAX, "%.2f %s / %.2f %s\n%.2f %s/s", util_get_display_size(updateData->installInfo.currProcessed), util_get_display_size_units(updateData->installInfo.currProcessed), util_get_display_size(updateData->installInfo.currTotal), util_get_display_size_units(updateData->installInfo.currTotal), util_get_display_size(updateData->installInfo.copyBytesPerSecond), util_get_display_size_units(updateData->installInfo.copyBytesPerSecond));
}

static void update_check_update(ui_view* view, void* data, float* progress, char* text) {
    update_data* updateData = (update_data*) data;

    bool hasUpdate = false;

    Result res = 0;
    u32 responseCode = 0;

    httpcContext context;
    if(R_SUCCEEDED(res = util_http_open(&context, &responseCode, "https://api.github.com/repos/Steveice10/FBI/releases/latest", true))) {
        u32 size = 0;
        if(R_SUCCEEDED(res = util_http_get_size(&context, &size))) {
            char* jsonText = (char*) calloc(sizeof(char), size);
            if(jsonText != NULL) {
                u32 bytesRead = 0;
                if(R_SUCCEEDED(res = util_http_read(&context, &bytesRead, (u8*) jsonText, size))) {
                    json_value* json = json_parse(jsonText, size);
                    if(json != NULL) {
                        if(json->type == json_object) {
                            json_value* name = NULL;
                            json_value* assets = NULL;

                            for(u32 i = 0; i < json->u.object.length; i++) {
                                json_value* val = json->u.object.values[i].value;
                                if(strncmp(json->u.object.values[i].name, "name", json->u.object.values[i].name_length) == 0 && val->type == json_string) {
                                    name = val;
                                } else if(strncmp(json->u.object.values[i].name, "assets", json->u.object.values[i].name_length) == 0 && val->type == json_array) {
                                    assets = val;
                                }
                            }

                            if(name != NULL && assets != NULL) {
                                char versionString[16];
                                snprintf(versionString, sizeof(versionString), "%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO);

                                if(strncmp(name->u.string.ptr, versionString, name->u.string.length) != 0) {
                                    char* url = NULL;

                                    for(u32 i = 0; i < assets->u.array.length; i++) {
                                        json_value* val = assets->u.array.values[i];
                                        if(val->type == json_object) {
                                            json_value* assetName = NULL;
                                            json_value* assetUrl = NULL;

                                            for(u32 j = 0; j < val->u.object.length; j++) {
                                                json_value* subVal = val->u.object.values[j].value;
                                                if(strncmp(val->u.object.values[j].name, "name", val->u.object.values[j].name_length) == 0 && subVal->type == json_string) {
                                                    assetName = subVal;
                                                } else if(strncmp(val->u.object.values[j].name, "browser_download_url", val->u.object.values[j].name_length) == 0 && subVal->type == json_string) {
                                                    assetUrl = subVal;
                                                }
                                            }

                                            if(assetName != NULL && assetUrl != NULL) {
                                                if(strncmp(assetName->u.string.ptr, util_get_3dsx_path() != NULL ? "FBI.3dsx" : "FBI.cia", assetName->u.string.length) == 0) {
                                                    url = assetUrl->u.string.ptr;
                                                    break;
                                                }
                                            }
                                        }
                                    }

                                    if(url != NULL) {
                                        strncpy(updateData->url, url, URL_MAX);
                                        hasUpdate = true;
                                    } else {
                                        res = R_FBI_BAD_DATA;
                                    }
                                }
                            } else {
                                res = R_FBI_BAD_DATA;
                            }
                        } else {
                            res = R_FBI_BAD_DATA;
                        }
                    } else {
                        res = R_FBI_PARSE_FAILED;
                    }
                }

                free(jsonText);
            } else {
                res = R_FBI_OUT_OF_MEMORY;
            }
        }

        Result closeRes = util_http_close(&context);
        if(R_SUCCEEDED(res)) {
            res = closeRes;
        }
    }

    ui_pop();
    info_destroy(view);

    if(hasUpdate) {
        if(R_SUCCEEDED(res = task_data_op(&updateData->installInfo))) {
            info_display("Updating FBI", "Press B to cancel.", true, data, update_install_update, NULL);
        } else {
            error_display_res(NULL, NULL, res, "Failed to begin update.");
        }
    } else {
        if(R_FAILED(res)) {
            if(res == R_FBI_HTTP_RESPONSE_CODE) {
                error_display(NULL, NULL, "Failed to check for update.\nHTTP server returned response code %d", responseCode);
            } else {
                error_display_res(NULL, NULL, res, "Failed to check for update.");
            }
        } else {
            prompt_display_notify("Success", "No updates available.", COLOR_TEXT, NULL, NULL, NULL);
        }

        free(data);
    }
}

static void update_onresponse(ui_view* view, void* data, u32 response) {
    if(response == PROMPT_YES) {
        info_display("Checking For Updates", "", false, data, update_check_update, NULL);
    } else {
        free(data);
    }
}

void update_open() {
    update_data* data = (update_data*) calloc(1, sizeof(update_data));
    if(data == NULL) {
        error_display(NULL, NULL, "Failed to allocate update check data.");

        return;
    }

    data->responseCode = 0;

    data->installInfo.data = data;

    data->installInfo.op = DATAOP_COPY;

    data->installInfo.copyBufferSize = 128 * 1024;
    data->installInfo.copyEmpty = false;

    data->installInfo.total = 1;

    data->installInfo.isSrcDirectory = update_is_src_directory;
    data->installInfo.makeDstDirectory = update_make_dst_directory;

    data->installInfo.openSrc = update_open_src;
    data->installInfo.closeSrc = update_close_src;
    data->installInfo.getSrcSize = update_get_src_size;
    data->installInfo.readSrc = update_read_src;

    data->installInfo.openDst = update_open_dst;
    data->installInfo.closeDst = update_close_dst;
    data->installInfo.writeDst = update_write_dst;

    data->installInfo.suspendCopy = update_suspend_copy;
    data->installInfo.restoreCopy = update_restore_copy;

    data->installInfo.suspend = update_suspend;
    data->installInfo.restore = update_restore;

    data->installInfo.error = update_error;

    data->installInfo.finished = true;

    prompt_display_yes_no("Confirmation", "Check for FBI updates?", COLOR_TEXT, data, NULL, update_onresponse);
}
