#include <sys/syslimits.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <3ds.h>

#include "../../list.h"
#include "../../error.h"
#include "../../../util.h"
#include "task.h"

typedef struct {
    list_item* items;
    u32* count;
    u32 max;

    Handle cancelEvent;
} populate_pending_titles_data;

static Result task_populate_pending_titles_from(populate_pending_titles_data* data, FS_MediaType mediaType) {
    Result res = 0;

    u32 pendingTitleCount = 0;
    if(R_SUCCEEDED(res = AM_GetPendingTitleCount(&pendingTitleCount, mediaType, AM_STATUS_MASK_INSTALLING | AM_STATUS_MASK_AWAITING_FINALIZATION))) {
        u64* pendingTitleIds = (u64*) calloc(pendingTitleCount, sizeof(u64));
        if(pendingTitleIds != NULL) {
            if(R_SUCCEEDED(res = AM_GetPendingTitleList(&pendingTitleCount, pendingTitleCount, mediaType, AM_STATUS_MASK_INSTALLING | AM_STATUS_MASK_AWAITING_FINALIZATION, pendingTitleIds))) {
                qsort(pendingTitleIds, pendingTitleCount, sizeof(u64), util_compare_u64);

                AM_PendingTitleEntry* pendingTitleInfos = (AM_PendingTitleEntry*) calloc(pendingTitleCount, sizeof(AM_PendingTitleEntry));
                if(pendingTitleInfos != NULL) {
                    if(R_SUCCEEDED(res = AM_GetPendingTitleInfo(pendingTitleCount, mediaType, pendingTitleIds, pendingTitleInfos))) {
                        for(u32 i = 0; i < pendingTitleCount && i < data->max; i++) {
                            if(task_is_quit_all() || svcWaitSynchronization(data->cancelEvent, 0) == 0) {
                                break;
                            }

                            pending_title_info* pendingTitleInfo = (pending_title_info*) calloc(1, sizeof(pending_title_info));
                            if(pendingTitleInfo != NULL) {
                                pendingTitleInfo->mediaType = mediaType;
                                pendingTitleInfo->titleId = pendingTitleIds[i];
                                pendingTitleInfo->version = pendingTitleInfos[i].version;

                                list_item* item = &data->items[*data->count];
                                snprintf(item->name, NAME_MAX, "%016llX", pendingTitleIds[i]);
                                if(mediaType == MEDIATYPE_NAND) {
                                    item->rgba = 0xFF0000FF;
                                } else if(mediaType == MEDIATYPE_SD) {
                                    item->rgba = 0xFF00FF00;
                                }

                                item->data = pendingTitleInfo;

                                (*data->count)++;
                            }
                        }
                    }

                    free(pendingTitleInfos);
                } else {
                    res = MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, 254, RD_OUT_OF_MEMORY);
                }
            }

            free(pendingTitleIds);
        } else {
            res = MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, 254, RD_OUT_OF_MEMORY);
        }
    }

    return res;
}

static void task_populate_pending_titles_thread(void* arg) {
    populate_pending_titles_data* data = (populate_pending_titles_data*) arg;

    Result res = 0;
    if(R_FAILED(res = task_populate_pending_titles_from(data, MEDIATYPE_SD)) || R_FAILED(res = task_populate_pending_titles_from(data, MEDIATYPE_NAND))) {
        error_display_res(NULL, NULL, res, "Failed to load pending title listing.");
    }

    svcCloseHandle(data->cancelEvent);
    free(data);
}

static void task_clear_pending_titles(list_item* items, u32* count) {
    if(items == NULL || count == NULL) {
        return;
    }

    u32 prevCount = *count;
    *count = 0;

    for(u32 i = 0; i < prevCount; i++) {
        if(items[i].data != NULL) {
            free(items[i].data);
            items[i].data = NULL;
        }

        memset(items[i].name, '\0', NAME_MAX);
        items[i].rgba = 0;
    }
}

Handle task_populate_pending_titles(list_item* items, u32* count, u32 max) {
    if(items == NULL || count == NULL || max == 0) {
        return 0;
    }

    task_clear_pending_titles(items, count);

    populate_pending_titles_data* data = (populate_pending_titles_data*) calloc(1, sizeof(populate_pending_titles_data));
    data->items = items;
    data->count = count;
    data->max = max;

    Result eventRes = svcCreateEvent(&data->cancelEvent, 1);
    if(R_FAILED(eventRes)) {
        error_display_res(NULL, NULL, eventRes, "Failed to create pending title list cancel event.");

        free(data);
        return 0;
    }

    if(threadCreate(task_populate_pending_titles_thread, data, 0x4000, 0x18, 1, true) == NULL) {
        error_display(NULL, NULL, "Failed to create pending title list thread.");

        svcCloseHandle(data->cancelEvent);
        free(data);
        return 0;
    }

    return data->cancelEvent;
}