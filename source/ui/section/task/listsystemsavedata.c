#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>

#include <3ds.h>

#include "task.h"
#include "../../list.h"
#include "../../error.h"
#include "../../../core/linkedlist.h"
#include "../../../core/screen.h"
#include "../../../core/util.h"

#define MAX_SYSTEM_SAVE_DATA 512

typedef struct {
    linked_list* items;

    Handle cancelEvent;
} populate_system_save_data_data;

static void task_populate_system_save_data_thread(void* arg) {
    populate_system_save_data_data* data = (populate_system_save_data_data*) arg;

    Result res = 0;

    u32 systemSaveDataCount = 0;
    u32 systemSaveDataIds[MAX_SYSTEM_SAVE_DATA];
    if(R_SUCCEEDED(res = FSUSER_EnumerateSystemSaveData(&systemSaveDataCount, MAX_SYSTEM_SAVE_DATA * sizeof(u32), systemSaveDataIds))) {
        qsort(systemSaveDataIds, systemSaveDataCount, sizeof(u32), util_compare_u32);

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

                    res = R_FBI_OUT_OF_MEMORY;
                }
            } else {
                res = R_FBI_OUT_OF_MEMORY;
            }
        }
    }

    if(R_FAILED(res)) {
        error_display_res(NULL, NULL, NULL, res, "Failed to load system save data listing.");
    }

    svcCloseHandle(data->cancelEvent);
    free(data);
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
        task_free_system_save_data(item);
        linked_list_iter_remove(&iter);
    }
}

Handle task_populate_system_save_data(linked_list* items) {
    if(items == NULL) {
        return 0;
    }

    task_clear_system_save_data(items);

    populate_system_save_data_data* data = (populate_system_save_data_data*) calloc(1, sizeof(populate_system_save_data_data));
    if(data == NULL) {
        error_display(NULL, NULL, NULL, "Failed to allocate system save data list data.");

        return 0;
    }

    data->items = items;

    Result eventRes = svcCreateEvent(&data->cancelEvent, 1);
    if(R_FAILED(eventRes)) {
        error_display_res(NULL, NULL, NULL, eventRes, "Failed to create system save data list cancel event.");

        free(data);
        return 0;
    }

    if(threadCreate(task_populate_system_save_data_thread, data, 0x10000, 0x18, 1, true) == NULL) {
        error_display(NULL, NULL, NULL, "Failed to create system save data list thread.");

        svcCloseHandle(data->cancelEvent);
        free(data);
        return 0;
    }

    return data->cancelEvent;
}