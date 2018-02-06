#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <3ds.h>

#include "../action/action.h"
#include "../task/uitask.h"
#include "../../error.h"
#include "../../info.h"
#include "../../prompt.h"
#include "../../resources.h"
#include "../../ui.h"
#include "../../../core/error.h"
#include "../../../core/http.h"
#include "../../../core/screen.h"
#include "../../../core/util.h"
#include "../../../core/data/cia.h"
#include "../../../core/data/ticket.h"

typedef enum content_type_e {
    CONTENT_CIA,
    CONTENT_TICKET,
    CONTENT_3DSX
} content_type;

typedef struct {
    char urls[INSTALL_URLS_MAX][DOWNLOAD_URL_MAX];

    char path3dsx[FILE_PATH_MAX];

    void* userData;
    void (*finished)(void* data);
    void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2, u32 index);

    bool cdn;
    bool selectCdnVersion;
    bool cdnDecided;

    content_type contentType;
    u64 currTitleId;
    volatile bool n3dsContinue;
    ticket_info ticketInfo;
    httpcContext* currContext;
    char curr3dsxPath[FILE_PATH_MAX];

    data_op_data installInfo;
} install_url_data;

static void action_install_url_free_data(install_url_data* data) {
    if(data->finished != NULL) {
        data->finished(data->userData);
    }

    free(data);
}

#define CDN_PROMPT_DEFAULT_VERSION 0
#define CDN_PROMPT_SELECT_VERSION 1
#define CDN_PROMPT_NO 2

static void action_install_url_cdn_check_onresponse(ui_view* view, void* data, u32 response) {
    install_url_data* installData = (install_url_data*) data;

    installData->cdn = response != CDN_PROMPT_NO;
    installData->selectCdnVersion = response == CDN_PROMPT_SELECT_VERSION;
    installData->cdnDecided = true;
}

static void action_install_url_n3ds_onresponse(ui_view* view, void* data, u32 response) {
    ((install_url_data*) data)->n3dsContinue = response == PROMPT_YES;
}

static void action_install_url_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    install_url_data* installData = (install_url_data*) data;

    if(installData->drawTop != NULL) {
        installData->drawTop(view, installData->userData, x1, y1, x2, y2, installData->installInfo.processed);
    } else if(installData->installInfo.processed == installData->installInfo.total) {
        float urlY = y1 + 5;
        u32 index = 0;
        while(urlY < y2 && index < installData->installInfo.total) {
            float urlWidth = 0;
            float urlHeight = 0;
            screen_get_string_size_wrap(&urlWidth, &urlHeight, installData->urls[index], 0.5f, 0.5f, x2 - x1 - 10);

            float urlX = x1 + (x2 - x1 - urlWidth) / 2;
            screen_draw_string_wrap(installData->urls[index], urlX, urlY, 0.5f, 0.5f, COLOR_TEXT, true, urlX + urlWidth + 1);

            urlY += urlHeight;
            index++;
        }
    } else {
        float urlWidth = 0;
        float urlHeight = 0;
        screen_get_string_size_wrap(&urlWidth, &urlHeight, installData->urls[installData->installInfo.processed], 0.5f, 0.5f, x2 - x1 - 10);

        float urlX = x1 + (x2 - x1 - urlWidth) / 2;
        float urlY = y1 + (y2 - y1 - urlHeight) / 2;
        screen_draw_string_wrap(installData->urls[installData->installInfo.processed], urlX, urlY, 0.5f, 0.5f, COLOR_TEXT, true, urlX + urlWidth + 1);
    }
}

static Result action_install_url_is_src_directory(void* data, u32 index, bool* isDirectory) {
    *isDirectory = false;
    return 0;
}

static Result action_install_url_make_dst_directory(void* data, u32 index) {
    return 0;
}

static Result action_install_url_open_src(void* data, u32 index, u32* handle) {
    install_url_data* installData = (install_url_data*) data;

    Result res = 0;

    httpcContext* context = (httpcContext*) calloc(1, sizeof(httpcContext));
    if(context != NULL) {
        if(R_SUCCEEDED(res = http_open(context, installData->urls[index], true))) {
            *handle = (u32) context;

            installData->currContext = context;
        } else {
            free(context);
        }
    } else {
        res = R_APP_OUT_OF_MEMORY;
    }

    return res;
}

static Result action_install_url_close_src(void* data, u32 index, bool succeeded, u32 handle) {
    ((install_url_data*) data)->currContext = NULL;

    return http_close((httpcContext*) handle);
}

static Result action_install_url_get_src_size(void* data, u32 handle, u64* size) {
    u32 downloadSize = 0;
    Result res = http_get_size((httpcContext*) handle, &downloadSize);

    *size = downloadSize;
    return res;
}

static Result action_install_url_read_src(void* data, u32 handle, u32* bytesRead, void* buffer, u64 offset, u32 size) {
    return http_read((httpcContext*) handle, bytesRead, buffer, size);
}


static Result action_install_url_open_dst(void* data, u32 index, void* initialReadBlock, u64 size, u32* handle) {
    install_url_data* installData = (install_url_data*) data;

    Result res = 0;

    installData->contentType = CONTENT_CIA;
    installData->currTitleId = 0;
    installData->n3dsContinue = false;
    memset(&installData->ticketInfo, 0, sizeof(installData->ticketInfo));
    memset(&installData->curr3dsxPath, 0, sizeof(installData->curr3dsxPath));

    if(*(u16*) initialReadBlock == 0x2020) {
        installData->contentType = CONTENT_CIA;

        u64 titleId = cia_get_title_id((u8*) initialReadBlock);

        FS_MediaType dest = util_get_title_destination(titleId);

        bool n3ds = false;
        if(R_SUCCEEDED(APT_CheckNew3DS(&n3ds)) && !n3ds && ((titleId >> 28) & 0xF) == 2) {
            ui_view* view = prompt_display_yes_no("Confirmation", "Title is intended for New 3DS systems.\nContinue?", COLOR_TEXT, data, action_install_url_draw_top, action_install_url_n3ds_onresponse);
            if(view != NULL) {
                svcWaitSynchronization(view->active, U64_MAX);
            }

            if(!installData->n3dsContinue) {
                return R_APP_SKIPPED;
            }
        }

        // Deleting FBI before it reinstalls itself causes issues.
        u64 currTitleId = 0;
        FS_MediaType currMediaType = MEDIATYPE_NAND;

        if(envIsHomebrew() || R_FAILED(APT_GetAppletInfo((NS_APPID) envGetAptAppId(), &currTitleId, (u8*) &currMediaType, NULL, NULL, NULL)) || titleId != currTitleId || dest != currMediaType) {
            AM_DeleteTitle(dest, titleId);
            AM_DeleteTicket(titleId);

            if(dest == MEDIATYPE_SD) {
                AM_QueryAvailableExternalTitleDatabase(NULL);
            }
        }

        if(R_SUCCEEDED(res = AM_StartCiaInstall(dest, handle))) {
            installData->currTitleId = titleId;
        }
    } else if(*(u16*) initialReadBlock == 0x0100) {
        installData->contentType = CONTENT_TICKET;

        if(!installData->cdnDecided) {
            static const char* options[3] = {"Default\nVersion", "Select\nVersion", "No"};
            static u32 optionButtons[3] = {KEY_A, KEY_X, KEY_B};
            ui_view* view = prompt_display_multi_choice("Optional", "Install ticket titles from CDN?", COLOR_TEXT, options, optionButtons, 3, data, action_install_url_draw_top, action_install_url_cdn_check_onresponse);
            if(view != NULL) {
                svcWaitSynchronization(view->active, U64_MAX);
            }
        }

        installData->ticketInfo.titleId = ticket_get_title_id((u8*) initialReadBlock);
        installData->ticketInfo.inUse = false;

        AM_DeleteTicket(installData->ticketInfo.titleId);
        res = AM_InstallTicketBegin(handle);
    } else if(*(u32*) initialReadBlock == 0x58534433) {
        installData->contentType = CONTENT_3DSX;

        FS_Archive sdmcArchive = 0;
        if(R_SUCCEEDED(res = FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, "")))) {
            char dir[FILE_PATH_MAX];
            if(strlen(installData->path3dsx) > 0) {
                util_get_parent_path(dir, installData->path3dsx, FILE_PATH_MAX);
                strncpy(installData->curr3dsxPath, installData->path3dsx, FILE_PATH_MAX);
            } else {
                char filename[FILE_NAME_MAX];
                if(R_FAILED(http_get_file_name(installData->currContext, filename, FILE_NAME_MAX))) {
                    util_get_path_file(filename, installData->urls[index], FILE_NAME_MAX);
                }

                char name[FILE_NAME_MAX];
                util_get_file_name(name, filename, FILE_NAME_MAX);

                snprintf(dir, FILE_PATH_MAX, "/3ds/%s/", name);
                snprintf(installData->curr3dsxPath, FILE_PATH_MAX, "/3ds/%s/%s.3dsx", name, name);
            }

            if(R_SUCCEEDED(res = util_ensure_dir(sdmcArchive, "/3ds/")) && R_SUCCEEDED(res = util_ensure_dir(sdmcArchive, dir))) {
                FS_Path* path = util_make_path_utf8(installData->curr3dsxPath);
                if(path != NULL) {
                    res = FSUSER_OpenFileDirectly(handle, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), *path, FS_OPEN_WRITE | FS_OPEN_CREATE, 0);

                    util_free_path_utf8(path);
                } else {
                    res = R_APP_OUT_OF_MEMORY;
                }
            }

            FSUSER_CloseArchive(sdmcArchive);
        }
    } else {
        res = R_APP_BAD_DATA;
    }

    return res;
}

static Result action_install_url_close_dst(void* data, u32 index, bool succeeded, u32 handle) {
    install_url_data* installData = (install_url_data*) data;

    Result res = 0;

    if(succeeded) {
        if(installData->contentType == CONTENT_CIA) {
            if(R_SUCCEEDED(res = AM_FinishCiaInstall(handle))) {
                task_download_seed_sync(installData->currTitleId);

                if(installData->currTitleId == 0x0004013800000002 || installData->currTitleId == 0x0004013820000002) {
                    res = AM_InstallFirm(installData->currTitleId);
                }
            }
        } else if(installData->contentType == CONTENT_TICKET) {
            res = AM_InstallTicketFinish(handle);

            if(R_SUCCEEDED(res) && installData->cdn) {
                volatile bool done = false;
                action_install_cdn_noprompt(&done, &installData->ticketInfo, false, installData->selectCdnVersion);

                while(!done) {
                    svcSleepThread(100000000);
                }
            }
        } else if(installData->contentType == CONTENT_3DSX) {
            res = FSFILE_Close(handle);
        }
    } else {
        if(installData->contentType == CONTENT_CIA) {
            res = AM_CancelCIAInstall(handle);
        } else if(installData->contentType == CONTENT_TICKET) {
            res = AM_InstallTicketAbort(handle);
        } else if(installData->contentType == CONTENT_3DSX) {
            res = FSFILE_Close(handle);

            FS_Archive sdmcArchive = 0;
            if(R_SUCCEEDED(FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, "")))) {
                FS_Path* path = util_make_path_utf8(installData->curr3dsxPath);
                if(path != NULL) {
                    FSUSER_DeleteFile(sdmcArchive, *path);

                    util_free_path_utf8(path);
                }

                FSUSER_CloseArchive(sdmcArchive);
            }
        }
    }

    return res;
}

static Result action_install_url_write_dst(void* data, u32 handle, u32* bytesWritten, void* buffer, u64 offset, u32 size) {
    return FSFILE_Write(handle, bytesWritten, offset, buffer, size, 0);
}

static Result action_install_url_suspend_transfer(void* data, u32 index, u32* srcHandle, u32* dstHandle) {
    return 0;
}

static Result action_install_url_restore_transfer(void* data, u32 index, u32* srcHandle, u32* dstHandle) {
    return 0;
}

static Result action_install_url_suspend(void* data, u32 index) {
    return 0;
}

static Result action_install_url_restore(void* data, u32 index) {
    return 0;
}

static bool action_install_url_error(void* data, u32 index, Result res, ui_view** errorView) {
    install_url_data* installData = (install_url_data*) data;

    char* url = installData->urls[index];
    if(strlen(url) > 38) {
        *errorView = error_display_res(data, action_install_url_draw_top, res, "Failed to install from URL.\n%.35s...", url);
    } else {
        *errorView = error_display_res(data, action_install_url_draw_top, res, "Failed to install from URL.\n%.38s", url);
    }

    return true;
}

static void action_install_url_install_update(ui_view* view, void* data, float* progress, char* text) {
    install_url_data* installData = (install_url_data*) data;

    if(installData->installInfo.finished) {
        ui_pop();
        info_destroy(view);

        if(R_SUCCEEDED(installData->installInfo.result)) {
            prompt_display_notify("Success", "Install finished.", COLOR_TEXT, NULL, NULL, NULL);
        }

        action_install_url_free_data(installData);

        return;
    }

    if(hidKeysDown() & KEY_B) {
        svcSignalEvent(installData->installInfo.cancelEvent);
    }

    *progress = installData->installInfo.currTotal != 0 ? (float) ((double) installData->installInfo.currProcessed / (double) installData->installInfo.currTotal) : 0;
    snprintf(text, PROGRESS_TEXT_MAX, "%lu / %lu\n%.2f %s / %.2f %s\n%.2f %s/s, ETA %s", installData->installInfo.processed, installData->installInfo.total,
             ui_get_display_size(installData->installInfo.currProcessed),
             ui_get_display_size_units(installData->installInfo.currProcessed),
             ui_get_display_size(installData->installInfo.currTotal),
             ui_get_display_size_units(installData->installInfo.currTotal),
             ui_get_display_size(installData->installInfo.bytesPerSecond),
             ui_get_display_size_units(installData->installInfo.bytesPerSecond),
             ui_get_display_eta(installData->installInfo.estimatedRemainingSeconds));
}

static void action_install_url_confirm_onresponse(ui_view* view, void* data, u32 response) {
    install_url_data* installData = (install_url_data*) data;

    if(response == PROMPT_YES) {
        Result res = task_data_op(&installData->installInfo);
        if(R_SUCCEEDED(res)) {
            info_display("Installing From URL(s)", "Press B to cancel.", true, data, action_install_url_install_update, action_install_url_draw_top);
        } else {
            error_display_res(NULL, NULL, res, "Failed to initiate installation.");

            action_install_url_free_data(installData);
        }
    } else {
        action_install_url_free_data(installData);
    }
}

void action_install_url(const char* confirmMessage, const char* urls, const char* path3dsx, void* userData, void (*finished)(void* data),
                                                                                                            void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2, u32 index)) {
    install_url_data* data = (install_url_data*) calloc(1, sizeof(install_url_data));
    if(data == NULL) {
        error_display(NULL, NULL, "Failed to allocate URL install data.");

        return;
    }

    data->installInfo.total = 0;

    size_t payloadLen = strlen(urls);
    if(payloadLen > 0) {
        const char* currStart = urls;
        while(data->installInfo.total < INSTALL_URLS_MAX && currStart - urls < payloadLen) {
            const char* currEnd = strchr(currStart, '\n');
            if(currEnd == NULL) {
                currEnd = urls + payloadLen;
            }

            u32 len = currEnd - currStart;

            if((len < 7 || strncmp(currStart, "http://", 7) != 0) && (len < 8 || strncmp(currStart, "https://", 8) != 0)) {
                if(len > DOWNLOAD_URL_MAX - 7) {
                    len = DOWNLOAD_URL_MAX - 7;
                }

                strncpy(data->urls[data->installInfo.total], "http://", 7);
                strncpy(&data->urls[data->installInfo.total][7], currStart, len);
            } else {
                if(len > DOWNLOAD_URL_MAX) {
                    len = DOWNLOAD_URL_MAX;
                }

                strncpy(data->urls[data->installInfo.total], currStart, len);
            }

            data->installInfo.total++;
            currStart = currEnd + 1;
        }
    }

    if(path3dsx != NULL) {
        strncpy(data->path3dsx, path3dsx, FILE_PATH_MAX);
    }

    data->userData = userData;
    data->finished = finished;
    data->drawTop = drawTop;

    data->cdn = false;
    data->selectCdnVersion = false;
    data->cdnDecided = false;

    data->contentType = CONTENT_CIA;
    data->currTitleId = 0;
    data->n3dsContinue = false;
    memset(&data->ticketInfo, 0, sizeof(data->ticketInfo));
    memset(&data->curr3dsxPath, 0, sizeof(data->curr3dsxPath));

    data->installInfo.data = data;

#ifdef USE_CURL
    data->installInfo.op = DATAOP_DOWNLOAD;

    data->installInfo.downloadUrls = data->urls;
#else
    data->installInfo.op = DATAOP_COPY;
#endif

    data->installInfo.bufferSize = 128 * 1024;
    data->installInfo.copyEmpty = false;

    data->installInfo.processed = data->installInfo.total;

    data->installInfo.isSrcDirectory = action_install_url_is_src_directory;
    data->installInfo.makeDstDirectory = action_install_url_make_dst_directory;

    data->installInfo.openSrc = action_install_url_open_src;
    data->installInfo.closeSrc = action_install_url_close_src;
    data->installInfo.getSrcSize = action_install_url_get_src_size;
    data->installInfo.readSrc = action_install_url_read_src;

    data->installInfo.openDst = action_install_url_open_dst;
    data->installInfo.closeDst = action_install_url_close_dst;
    data->installInfo.writeDst = action_install_url_write_dst;

    data->installInfo.suspendTransfer = action_install_url_suspend_transfer;
    data->installInfo.restoreTransfer = action_install_url_restore_transfer;

    data->installInfo.suspend = action_install_url_suspend;
    data->installInfo.restore = action_install_url_restore;

    data->installInfo.error = action_install_url_error;

    data->installInfo.finished = true;

    prompt_display_yes_no("Confirmation", confirmMessage, COLOR_TEXT, data, action_install_url_draw_top, action_install_url_confirm_onresponse);
}