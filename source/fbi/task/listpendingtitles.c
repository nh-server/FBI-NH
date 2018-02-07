#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>

#include <3ds.h>

#include "listpendingtitles.h"
#include "../resources.h"
#include "../../core/core.h"

static int task_populate_pending_titles_compare_ids(const void* e1, const void* e2) {
    u64 id1 = *(u64*) e1;
    u64 id2 = *(u64*) e2;

    return id1 > id2 ? 1 : id1 < id2 ? -1 : 0;
}

static Result task_populate_pending_titles_from(populate_pending_titles_data* data, FS_MediaType mediaType) {
    Result res = 0;

    u32 pendingTitleCount = 0;
    if(R_SUCCEEDED(res = AM_GetPendingTitleCount(&pendingTitleCount, mediaType, AM_STATUS_MASK_INSTALLING | AM_STATUS_MASK_AWAITING_FINALIZATION))) {
        u64* pendingTitleIds = (u64*) calloc(pendingTitleCount, sizeof(u64));
        if(pendingTitleIds != NULL) {
            if(R_SUCCEEDED(res = AM_GetPendingTitleList(&pendingTitleCount, pendingTitleCount, mediaType, AM_STATUS_MASK_INSTALLING | AM_STATUS_MASK_AWAITING_FINALIZATION, pendingTitleIds))) {
                qsort(pendingTitleIds, pendingTitleCount, sizeof(u64), task_populate_pending_titles_compare_ids);

                AM_PendingTitleEntry* pendingTitleInfos = (AM_PendingTitleEntry*) calloc(pendingTitleCount, sizeof(AM_PendingTitleEntry));
                if(pendingTitleInfos != NULL) {
                    if(R_SUCCEEDED(res = AM_GetPendingTitleInfo(pendingTitleCount, mediaType, pendingTitleIds, pendingTitleInfos))) {
                        for(u32 i = 0; i < pendingTitleCount && R_SUCCEEDED(res); i++) {
                            svcWaitSynchronization(task_get_pause_event(), U64_MAX);
                            if(task_is_quit_all() || svcWaitSynchronization(data->cancelEvent, 0) == 0) {
                                break;
                            }

                            list_item* item = (list_item*) calloc(1, sizeof(list_item));
                            if(item != NULL) {
                                pending_title_info* pendingTitleInfo = (pending_title_info*) calloc(1, sizeof(pending_title_info));
                                if(pendingTitleInfo != NULL) {
                                    pendingTitleInfo->mediaType = mediaType;
                                    pendingTitleInfo->titleId = pendingTitleIds[i];
                                    pendingTitleInfo->version = pendingTitleInfos[i].version;

                                    snprintf(item->name, LIST_ITEM_NAME_MAX, "%016llX", pendingTitleIds[i]);
                                    if(mediaType == MEDIATYPE_NAND) {
                                        item->color = COLOR_NAND;
                                    } else if(mediaType == MEDIATYPE_SD) {
                                        item->color = COLOR_SD;
                                    }

                                    item->data = pendingTitleInfo;

                                    linked_list_add(data->items, item);
                                } else {
                                    free(item);

                                    res = R_APP_OUT_OF_MEMORY;
                                }
                            } else {
                                res = R_APP_OUT_OF_MEMORY;
                            }
                        }
                    }

                    free(pendingTitleInfos);
                } else {
                    res = R_APP_OUT_OF_MEMORY;
                }
            }

            free(pendingTitleIds);
        } else {
            res = R_APP_OUT_OF_MEMORY;
        }
    }

    return res;
}

static void task_populate_pending_titles_thread(void* arg) {
    populate_pending_titles_data* data = (populate_pending_titles_data*) arg;

    Result res = 0;

    if(R_SUCCEEDED(res = task_populate_pending_titles_from(data, MEDIATYPE_SD))) {
        res = task_populate_pending_titles_from(data, MEDIATYPE_NAND);
    }

    svcCloseHandle(data->cancelEvent);

    data->result = res;
    data->finished = true;
}

void task_free_pending_title(list_item* item) {
    if(item == NULL) {
        return;
    }

    if(item->data != NULL) {
        free(item->data);
    }

    free(item);
}

void task_clear_pending_titles(linked_list* items) {
    if(items == NULL) {
        return;
    }

    linked_list_iter iter;
    linked_list_iterate(items, &iter);

    while(linked_list_iter_has_next(&iter)) {
        list_item* item = (list_item*) linked_list_iter_next(&iter);

        linked_list_iter_remove(&iter);
        task_free_pending_title(item);
    }
}

Result task_populate_pending_titles(populate_pending_titles_data* data) {
    if(data == NULL || data->items == NULL) {
        return R_APP_INVALID_ARGUMENT;
    }

    task_clear_pending_titles(data->items);

    data->finished = false;
    data->result = 0;
    data->cancelEvent = 0;

    Result res = 0;
    if(R_SUCCEEDED(res = svcCreateEvent(&data->cancelEvent, RESET_STICKY))) {
        if(threadCreate(task_populate_pending_titles_thread, data, 0x10000, 0x19, 1, true) == NULL) {
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

    return res;
}