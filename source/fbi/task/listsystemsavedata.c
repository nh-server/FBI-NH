#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>

#include <3ds.h>

#include "listsystemsavedata.h"
#include "../resources.h"
#include "../../core/core.h"

#define MAX_SYSTEM_SAVE_DATA 512

static int task_populate_system_save_data_compare_ids(const void* e1, const void* e2) {
    u32 id1 = *(u32*) e1;
    u32 id2 = *(u32*) e2;

    return id1 > id2 ? 1 : id1 < id2 ? -1 : 0;
}

static void task_populate_system_save_data_thread(void* arg) {
    populate_system_save_data_data* data = (populate_system_save_data_data*) arg;

    Result res = 0;

    u32 systemSaveDataCount = 0;
    u32 systemSaveDataIds[MAX_SYSTEM_SAVE_DATA];
    if(R_SUCCEEDED(res = FSUSER_EnumerateSystemSaveData(&systemSaveDataCount, MAX_SYSTEM_SAVE_DATA * sizeof(u32), systemSaveDataIds))) {
        qsort(systemSaveDataIds, systemSaveDataCount, sizeof(u32), task_populate_system_save_data_compare_ids);

        for(u32 i = 0; i < systemSaveDataCount && R_SUCCEEDED(res); i++) {
            svcWaitSynchronization(task_get_pause_event(), U64_MAX);
            if(task_is_quit_all() || svcWaitSynchronization(data->cancelEvent, 0) == 0) {
                break;
            }

            list_item* item = (list_item*) calloc(1, sizeof(list_item));
            if(item != NULL) {
                system_save_data_info* systemSaveDataInfo = (system_save_data_info*) calloc(1, sizeof(system_save_data_info));
                if(systemSaveDataInfo != NULL) {
                    systemSaveDataInfo->systemSaveDataId = systemSaveDataIds[i];

                    snprintf(item->name, LIST_ITEM_NAME_MAX, "%08lX", systemSaveDataIds[i]);
                    item->color = COLOR_TEXT;
                    item->data = systemSaveDataInfo;

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

    svcCloseHandle(data->cancelEvent);

    data->result = res;
    data->finished = true;
}

void task_free_system_save_data(list_item* item) {
    if(item == NULL) {
        return;
    }

    if(item->data != NULL) {
        free(item->data);
    }

    free(item);
}

void task_clear_system_save_data(linked_list* items) {
    if(items == NULL) {
        return;
    }

    linked_list_iter iter;
    linked_list_iterate(items, &iter);

    while(linked_list_iter_has_next(&iter)) {
        list_item* item = (list_item*) linked_list_iter_next(&iter);

        linked_list_iter_remove(&iter);
        task_free_system_save_data(item);
    }
}

Result task_populate_system_save_data(populate_system_save_data_data* data) {
    if(data == NULL || data->items == NULL) {
        return R_APP_INVALID_ARGUMENT;
    }

    task_clear_system_save_data(data->items);

    data->finished = false;
    data->result = 0;
    data->cancelEvent = 0;

    Result res = 0;
    if(R_SUCCEEDED(res = svcCreateEvent(&data->cancelEvent, RESET_STICKY))) {
        if(threadCreate(task_populate_system_save_data_thread, data, 0x10000, 0x19, 1, true) == NULL) {
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