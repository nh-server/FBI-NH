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
} populate_system_save_data_data;

static void task_populate_system_save_data_thread(void* arg) {
    populate_system_save_data_data* data = (populate_system_save_data_data*) arg;

    Result res = 0;

    u32 systemSaveDataCount = 0;
    u32* systemSaveDataIds = (u32*) calloc(data->max, sizeof(u32));
    if(systemSaveDataIds != NULL) {
        if(R_SUCCEEDED(res = FSUSER_EnumerateSystemSaveData(&systemSaveDataCount, data->max * sizeof(u32), systemSaveDataIds))) {
            qsort(systemSaveDataIds, systemSaveDataCount, sizeof(u32), util_compare_u32);

            for(u32 i = 0; i < systemSaveDataCount && i < data->max; i++) {
                if(task_is_quit_all() || svcWaitSynchronization(data->cancelEvent, 0) == 0) {
                    break;
                }

                system_save_data_info* systemSaveDataInfo = (system_save_data_info*) calloc(1, sizeof(system_save_data_info));
                if(systemSaveDataInfo != NULL) {
                    systemSaveDataInfo->systemSaveDataId = systemSaveDataIds[i];

                    list_item* item = &data->items[*data->count];
                    snprintf(item->name, NAME_MAX, "%08lX", systemSaveDataIds[i]);
                    item->rgba = 0xFF000000;
                    item->data = systemSaveDataInfo;

                    (*data->count)++;
                }
            }
        }

        free(systemSaveDataIds);
    } else {
        res = MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, 254, RD_OUT_OF_MEMORY);
    }

    if(R_FAILED(res)) {
        error_display_res(NULL, NULL, res, "Failed to load system save data listing.");
    }

    svcCloseHandle(data->cancelEvent);
    free(data);
}

static void task_clear_system_save_data(list_item* items, u32* count) {
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

Handle task_populate_system_save_data(list_item* items, u32* count, u32 max) {
    if(items == NULL || count == NULL || max == 0) {
        return 0;
    }

    task_clear_system_save_data(items, count);

    populate_system_save_data_data* data = (populate_system_save_data_data*) calloc(1, sizeof(populate_system_save_data_data));
    data->items = items;
    data->count = count;
    data->max = max;

    Result eventRes = svcCreateEvent(&data->cancelEvent, 1);
    if(R_FAILED(eventRes)) {
        error_display_res(NULL, NULL, eventRes, "Failed to create system save data list cancel event.");

        free(data);
        return 0;
    }

    if(threadCreate(task_populate_system_save_data_thread, data, 0x4000, 0x18, 1, true) == NULL) {
        error_display(NULL, NULL, "Failed to create system save data list thread.");

        svcCloseHandle(data->cancelEvent);
        free(data);
        return 0;
    }

    return data->cancelEvent;
}