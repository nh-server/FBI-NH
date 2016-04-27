#include <malloc.h>
#include <stdio.h>

#include <3ds.h>

#include "action.h"
#include "../task/task.h"
#include "../../error.h"
#include "../../info.h"
#include "../../list.h"
#include "../../prompt.h"
#include "../../ui.h"
#include "../../../core/linkedlist.h"
#include "../../../core/screen.h"

#define CONTENTS_MAX 64

typedef struct {
    ticket_info* ticket;

    u32 contentCount;
    u16 contentIndices[CONTENTS_MAX];
    u32 contentIds[CONTENTS_MAX];

    u32 responseCode;

    data_op_info installInfo;
    Handle cancelEvent;
} install_cdn_data;

static Result action_install_cdn_is_src_directory(void* data, u32 index, bool* isDirectory) {
    *isDirectory = false;
    return 0;
}

static Result action_install_cdn_make_dst_directory(void* data, u32 index) {
    return 0;
}

static Result action_install_cdn_open_src(void* data, u32 index, u32* handle) {
    install_cdn_data* installData = (install_cdn_data*) data;

    Result res = 0;

    httpcContext* context = (httpcContext*) calloc(1, sizeof(httpcContext));
    if(context != NULL) {
        char url[256];
        if(index == 0) {
            snprintf(url, 256, "http://ccs.cdn.c.shop.nintendowifi.net/ccs/download/%016llX/tmd", installData->ticket->titleId);
        } else {
            snprintf(url, 256, "http://ccs.cdn.c.shop.nintendowifi.net/ccs/download/%016llX/%08lX", installData->ticket->titleId, installData->contentIds[index - 1]);
        }

        if(R_SUCCEEDED(res = httpcOpenContext(context, HTTPC_METHOD_GET, url, 1))) {
            httpcSetSSLOpt(context, SSLCOPT_DisableVerify);
            if(R_SUCCEEDED(res = httpcBeginRequest(context)) && R_SUCCEEDED(res = httpcGetResponseStatusCode(context, &installData->responseCode, 0))) {
                if(installData->responseCode == 200) {
                    *handle = (u32) context;
                } else {
                    res = R_FBI_HTTP_RESPONSE_CODE;
                }
            }

            if(R_FAILED(res)) {
                httpcCloseContext(context);
            }
        }

        if(R_FAILED(res)) {
            free(context);
        }
    } else {
        res = R_FBI_OUT_OF_MEMORY;
    }

    return res;
}

static Result action_install_cdn_close_src(void* data, u32 index, bool succeeded, u32 handle) {
    return httpcCloseContext((httpcContext*) handle);
}

static Result action_install_cdn_get_src_size(void* data, u32 handle, u64* size) {
    u32 downloadSize = 0;
    Result res = httpcGetDownloadSizeState((httpcContext*) handle, NULL, &downloadSize);

    *size = downloadSize;
    return res;
}

static Result action_install_cdn_read_src(void* data, u32 handle, u32* bytesRead, void* buffer, u64 offset, u32 size) {
    Result res = httpcDownloadData((httpcContext*) handle, buffer, size, bytesRead);
    return res != HTTPC_RESULTCODE_DOWNLOADPENDING ? res : 0;
}

static Result action_install_cdn_open_dst(void* data, u32 index, void* initialReadBlock, u32* handle) {
    install_cdn_data* installData = (install_cdn_data*) data;

    if(index == 0) {
        static u32 dataOffsets[6] = {0x240, 0x140, 0x80, 0x240, 0x140, 0x80};

        u8* tmd = (u8*) initialReadBlock;
        u8 sigType = tmd[0x03];

        installData->contentCount = __builtin_bswap16(*(u16*) &tmd[dataOffsets[sigType] + 0x9E]);
        if(installData->contentCount > CONTENTS_MAX) {
            return MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_APPLICATION, RD_OUT_OF_RANGE);
        }

        for(u32 i = 0; i < installData->contentCount; i++) {
            u8* contentChunk = &tmd[dataOffsets[sigType] + 0x9C4 + (i * 0x30)];

            installData->contentIds[i] = __builtin_bswap32(*(u32*) &contentChunk[0x00]);
            installData->contentIndices[i] = __builtin_bswap16(*(u16*) &contentChunk[0x04]);
        }

        installData->installInfo.total += installData->contentCount;

        return AM_InstallTmdBegin(handle);
    } else {
        return AM_InstallContentBegin(handle, installData->contentIndices[index - 1]);
    }
}

static Result action_install_cdn_close_dst(void* data, u32 index, bool succeeded, u32 handle) {
    if(succeeded) {
        if(index == 0) {
            return AM_InstallTmdFinish(handle, true);
        } else {
            return AM_InstallContentFinish(handle);
        }
    } else {
        if(index == 0) {
            return AM_InstallTmdAbort(handle);
        } else {
            return AM_InstallContentCancel(handle);
        }
    }
}

static Result action_install_cdn_write_dst(void* data, u32 handle, u32* bytesWritten, void* buffer, u64 offset, u32 size) {
    return FSFILE_Write(handle, bytesWritten, offset, buffer, size, 0);
}

bool action_install_cdn_error(void* data, u32 index, Result res) {
    install_cdn_data* installData = (install_cdn_data*) data;

    if(res == R_FBI_CANCELLED) {
        prompt_display("Failure", "Install cancelled.", COLOR_TEXT, false, installData->ticket, NULL, ui_draw_ticket_info, NULL);
    } else if(res == R_FBI_HTTP_RESPONSE_CODE) {
        error_display(NULL, installData->ticket, ui_draw_ticket_info, "Failed to install CDN title.\nHTTP server returned response code %d", installData->responseCode);
    } else {
        error_display_res(NULL, installData->ticket, ui_draw_ticket_info, res, "Failed to install CDN title.");
    }

    return false;
}

static void action_install_cdn_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    ui_draw_ticket_info(view, ((install_cdn_data*) data)->ticket, x1, y1, x2, y2);
}

static void action_install_cdn_free_data(install_cdn_data* data) {
    free(data);
}

static void action_install_cdn_update(ui_view* view, void* data, float* progress, char* text) {
    install_cdn_data* installData = (install_cdn_data*) data;

    if(installData->installInfo.finished) {
        ui_pop();
        info_destroy(view);

        Result res = 0;

        if(!installData->installInfo.premature) {
            if(R_SUCCEEDED(res = AM_InstallTitleFinish())
               && R_SUCCEEDED(res = AM_CommitImportTitles(((installData->ticket->titleId >> 32) & 0x8010) != 0 ? MEDIATYPE_NAND : MEDIATYPE_SD, 1, false, &installData->ticket->titleId))) {
                if(installData->ticket->titleId == 0x0004013800000002 || installData->ticket->titleId == 0x0004013820000002) {
                    res = AM_InstallFirm(installData->ticket->titleId);
                }
            }
        }

        if(!installData->installInfo.premature && R_SUCCEEDED(res)) {
            prompt_display("Success", "Install finished.", COLOR_TEXT, false, installData->ticket, NULL, ui_draw_ticket_info, NULL);
        } else {
            AM_InstallTitleAbort();

            if(R_FAILED(res)) {
                error_display_res(NULL, installData->ticket, ui_draw_ticket_info, res, "Failed to install CDN title.");
            }
        }

        action_install_cdn_free_data(installData);

        return;
    }

    if(hidKeysDown() & KEY_B) {
        svcSignalEvent(installData->cancelEvent);
    }

    *progress = installData->installInfo.currTotal != 0 ? (float) ((double) installData->installInfo.currProcessed / (double) installData->installInfo.currTotal) : 0;
    snprintf(text, PROGRESS_TEXT_MAX, "%lu / %lu\n%.2f MB / %.2f MB", installData->installInfo.processed, installData->installInfo.total, installData->installInfo.currProcessed / 1024.0 / 1024.0, installData->installInfo.currTotal / 1024.0 / 1024.0);
}

static void action_install_cdn_onresponse(ui_view* view, void* data, bool response) {
    install_cdn_data* installData = (install_cdn_data*) data;

    if(response) {
        u8 n3ds = false;
        if(R_SUCCEEDED(APT_CheckNew3DS(&n3ds)) && !n3ds && ((installData->ticket->titleId >> 28) & 0xF) == 2) {
            error_display(NULL, installData->ticket, ui_draw_ticket_info, "Failed to install CDN title.\nAttempted to install N3DS title to O3DS.");

            action_install_cdn_free_data(installData);

            return;
        }

        FS_MediaType dest = ((installData->ticket->titleId >> 32) & 0x8010) != 0 ? MEDIATYPE_NAND : MEDIATYPE_SD;

        AM_DeleteTitle(dest, installData->ticket->titleId);
        if(dest == MEDIATYPE_SD) {
            AM_QueryAvailableExternalTitleDatabase(NULL);
        }

        Result res = 0;

        if(R_SUCCEEDED(res = AM_InstallTitleBegin(dest, installData->ticket->titleId, false))) {
            installData->cancelEvent = task_data_op(&installData->installInfo);
            if(installData->cancelEvent != 0) {
                info_display("Installing CDN Title", "Press B to cancel.", true, data, action_install_cdn_update, action_install_cdn_draw_top);
            } else {
                AM_InstallTitleAbort();
            }
        }

        if(R_FAILED(res) || installData->cancelEvent == 0) {
            error_display(NULL, installData->ticket, ui_draw_ticket_info, "Failed to initiate CDN title installation.");

            action_install_cdn_free_data(installData);
        }
    } else {
        action_install_cdn_free_data(installData);
    }
}

void action_install_cdn(linked_list* items, list_item* selected) {
    install_cdn_data* data = (install_cdn_data*) calloc(1, sizeof(install_cdn_data));
    if(data == NULL) {
        error_display(NULL, NULL, NULL, "Failed to allocate install CDN data.");

        return;
    }

    data->ticket = (ticket_info*) selected->data;

    data->responseCode = 0;

    data->installInfo.data = data;

    data->installInfo.op = DATAOP_COPY;

    data->installInfo.copyEmpty = false;

    data->installInfo.total = 1;

    data->installInfo.isSrcDirectory = action_install_cdn_is_src_directory;
    data->installInfo.makeDstDirectory = action_install_cdn_make_dst_directory;

    data->installInfo.openSrc = action_install_cdn_open_src;
    data->installInfo.closeSrc = action_install_cdn_close_src;
    data->installInfo.getSrcSize = action_install_cdn_get_src_size;
    data->installInfo.readSrc = action_install_cdn_read_src;

    data->installInfo.openDst = action_install_cdn_open_dst;
    data->installInfo.closeDst = action_install_cdn_close_dst;
    data->installInfo.writeDst = action_install_cdn_write_dst;

    data->installInfo.error = action_install_cdn_error;

    data->cancelEvent = 0;

    prompt_display("Confirmation", "Install the selected title from the CDN?", COLOR_TEXT, true, data, NULL, action_install_cdn_draw_top, action_install_cdn_onresponse);
}