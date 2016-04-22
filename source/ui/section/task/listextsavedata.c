#include <sys/syslimits.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <3ds.h>

#include "../../list.h"
#include "../../error.h"
#include "../../../screen.h"
#include "../../../util.h"
#include "task.h"

typedef struct {
    list_item* items;
    u32* count;
    u32 max;

    Handle cancelEvent;
} populate_ext_save_data_data;

static Result task_populate_ext_save_data_from(populate_ext_save_data_data* data, FS_MediaType mediaType) {
    Result res = 0;

    u32 extSaveDataCount = 0;
    u64* extSaveDataIds = (u64*) calloc(data->max, sizeof(u64));
    if(extSaveDataIds != NULL) {
        if(R_SUCCEEDED(res = FSUSER_EnumerateExtSaveData(&extSaveDataCount, data->max, mediaType, 8, mediaType == MEDIATYPE_NAND, (u8*) extSaveDataIds))) {
            qsort(extSaveDataIds, extSaveDataCount, sizeof(u64), util_compare_u64);

            SMDH smdh;
            for(u32 i = 0; i < extSaveDataCount && *data->count < data->max && R_SUCCEEDED(res); i++) {
                if(task_is_quit_all() || svcWaitSynchronization(data->cancelEvent, 0) == 0) {
                    break;
                }

                ext_save_data_info* extSaveDataInfo = (ext_save_data_info*) calloc(1, sizeof(ext_save_data_info));
                if(extSaveDataInfo != NULL) {
                    extSaveDataInfo->mediaType = mediaType;
                    extSaveDataInfo->extSaveDataId = extSaveDataIds[i];
                    extSaveDataInfo->shared = mediaType == MEDIATYPE_NAND;
                    extSaveDataInfo->hasMeta = false;

                    list_item* item = &data->items[*data->count];

                    FS_ExtSaveDataInfo info = {.mediaType = mediaType, .saveId = extSaveDataIds[i]};
                    u32 smdhBytesRead = 0;
                    if(R_SUCCEEDED(FSUSER_ReadExtSaveDataIcon(&smdhBytesRead, info, sizeof(SMDH), (u8*) &smdh)) && smdhBytesRead == sizeof(SMDH)) {
                        u8 systemLanguage = CFG_LANGUAGE_EN;
                        CFGU_GetSystemLanguage(&systemLanguage);

                        utf16_to_utf8((uint8_t*) item->name, smdh.titles[systemLanguage].shortDescription, NAME_MAX - 1);

                        extSaveDataInfo->hasMeta = true;
                        utf16_to_utf8((uint8_t*) extSaveDataInfo->meta.shortDescription, smdh.titles[systemLanguage].shortDescription, sizeof(extSaveDataInfo->meta.shortDescription) - 1);
                        utf16_to_utf8((uint8_t*) extSaveDataInfo->meta.longDescription, smdh.titles[systemLanguage].longDescription, sizeof(extSaveDataInfo->meta.longDescription) - 1);
                        utf16_to_utf8((uint8_t*) extSaveDataInfo->meta.publisher, smdh.titles[systemLanguage].publisher, sizeof(extSaveDataInfo->meta.publisher) - 1);
                        extSaveDataInfo->meta.texture = screen_load_texture_tiled_auto(smdh.largeIcon, sizeof(smdh.largeIcon), 48, 48, GPU_RGB565, false);
                    }

                    bool empty = strlen(item->name) == 0;
                    if(!empty) {
                        empty = true;

                        char* curr = item->name;
                        while(*curr) {
                            if(*curr != ' ') {
                                empty = false;
                                break;
                            }

                            curr++;
                        }
                    }

                    if(empty) {
                        snprintf(item->name, NAME_MAX, "%016llX", extSaveDataIds[i]);
                    }

                    if(mediaType == MEDIATYPE_NAND) {
                        item->rgba = COLOR_NAND;
                    } else if(mediaType == MEDIATYPE_SD) {
                        item->rgba = COLOR_SD;
                    }

                    item->data = extSaveDataInfo;

                    (*data->count)++;
                } else {
                    res = R_FBI_OUT_OF_MEMORY;
                }
            }
        }

        free(extSaveDataIds);
    } else {
        res = R_FBI_OUT_OF_MEMORY;
    }

    return res;
}

static void task_populate_ext_save_data_thread(void* arg) {
    populate_ext_save_data_data* data = (populate_ext_save_data_data*) arg;

    Result res = 0;
    if(R_FAILED(res = task_populate_ext_save_data_from(data, MEDIATYPE_SD)) || R_FAILED(res = task_populate_ext_save_data_from(data, MEDIATYPE_NAND))) {
        error_display_res(NULL, NULL, NULL, res, "Failed to load ext save data listing.");
    }

    svcCloseHandle(data->cancelEvent);
    free(data);
}

void task_clear_ext_save_data(list_item* items, u32* count) {
    if(items == NULL || count == NULL || *count == 0) {
        return;
    }

    u32 prevCount = *count;
    *count = 0;

    for(u32 i = 0; i < prevCount; i++) {
        if(items[i].data != NULL) {
            ext_save_data_info* extSaveDataInfo = (ext_save_data_info*) items[i].data;
            if(extSaveDataInfo->hasMeta) {
                screen_unload_texture(extSaveDataInfo->meta.texture);
            }

            free(items[i].data);
            items[i].data = NULL;
        }

        memset(items[i].name, '\0', NAME_MAX);
        items[i].rgba = 0;
    }
}

Handle task_populate_ext_save_data(list_item* items, u32* count, u32 max) {
    if(items == NULL || count == NULL || max == 0) {
        return 0;
    }

    task_clear_ext_save_data(items, count);

    populate_ext_save_data_data* data = (populate_ext_save_data_data*) calloc(1, sizeof(populate_ext_save_data_data));
    if(data == NULL) {
        error_display(NULL, NULL, NULL, "Failed to allocate ext save data list data.");

        return 0;
    }

    data->items = items;
    data->count = count;
    data->max = max;

    Result eventRes = svcCreateEvent(&data->cancelEvent, 1);
    if(R_FAILED(eventRes)) {
        error_display_res(NULL, NULL, NULL, eventRes, "Failed to create ext save data list cancel event.");

        free(data);
        return 0;
    }

    if(threadCreate(task_populate_ext_save_data_thread, data, 0x4000, 0x18, 1, true) == NULL) {
        error_display(NULL, NULL, NULL, "Failed to create ext save data list thread.");

        svcCloseHandle(data->cancelEvent);
        free(data);
        return 0;
    }

    return data->cancelEvent;
}