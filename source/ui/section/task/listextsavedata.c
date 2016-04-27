#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <3ds.h>

#include "task.h"
#include "../../list.h"
#include "../../error.h"
#include "../../../core/linkedlist.h"
#include "../../../core/screen.h"
#include "../../../core/util.h"

#define MAX_EXT_SAVE_DATA 512

typedef struct {
    linked_list* items;

    Handle cancelEvent;
} populate_ext_save_data_data;

static Result task_populate_ext_save_data_from(populate_ext_save_data_data* data, FS_MediaType mediaType) {
    Result res = 0;

    u32 extSaveDataCount = 0;
    u64 extSaveDataIds[MAX_EXT_SAVE_DATA];
    if(R_SUCCEEDED(res = FSUSER_EnumerateExtSaveData(&extSaveDataCount, MAX_EXT_SAVE_DATA, mediaType, 8, mediaType == MEDIATYPE_NAND, (u8*) extSaveDataIds))) {
        qsort(extSaveDataIds, extSaveDataCount, sizeof(u64), util_compare_u64);

        SMDH smdh;
        for(u32 i = 0; i < extSaveDataCount && R_SUCCEEDED(res); i++) {
            svcWaitSynchronization(task_get_pause_event(), U64_MAX);
            if(task_is_quit_all() || svcWaitSynchronization(data->cancelEvent, 0) == 0) {
                break;
            }

            list_item* item = (list_item*) calloc(1, sizeof(list_item));
            if(item != NULL) {
                ext_save_data_info* extSaveDataInfo = (ext_save_data_info*) calloc(1, sizeof(ext_save_data_info));
                if(extSaveDataInfo != NULL) {
                    extSaveDataInfo->mediaType = mediaType;
                    extSaveDataInfo->extSaveDataId = extSaveDataIds[i];
                    extSaveDataInfo->shared = mediaType == MEDIATYPE_NAND;
                    extSaveDataInfo->hasMeta = false;

                    FS_ExtSaveDataInfo info = {.mediaType = mediaType, .saveId = extSaveDataIds[i]};
                    u32 smdhBytesRead = 0;
                    if(R_SUCCEEDED(FSUSER_ReadExtSaveDataIcon(&smdhBytesRead, info, sizeof(SMDH), (u8*) &smdh)) && smdhBytesRead == sizeof(SMDH)) {
                        u8 systemLanguage = CFG_LANGUAGE_EN;
                        CFGU_GetSystemLanguage(&systemLanguage);

                        utf16_to_utf8((uint8_t*) item->name, smdh.titles[systemLanguage].shortDescription, LIST_ITEM_NAME_MAX - 1);

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
                        snprintf(item->name, LIST_ITEM_NAME_MAX, "%016llX", extSaveDataIds[i]);
                    }

                    if(mediaType == MEDIATYPE_NAND) {
                        item->color = COLOR_NAND;
                    } else if(mediaType == MEDIATYPE_SD) {
                        item->color = COLOR_SD;
                    }

                    item->data = extSaveDataInfo;

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

void task_free_ext_save_data(list_item* item) {
    if(item == NULL) {
        return;
    }

    if(item->data != NULL) {
        ext_save_data_info* extSaveDataInfo = (ext_save_data_info*) item->data;
        if(extSaveDataInfo->hasMeta) {
            screen_unload_texture(extSaveDataInfo->meta.texture);
        }

        free(item->data);
    }

    free(item);
}

void task_clear_ext_save_data(linked_list* items) {
    if(items == NULL) {
        return;
    }

    linked_list_iter iter;
    linked_list_iterate(items, &iter);

    while(linked_list_iter_has_next(&iter)) {
        list_item* item = (list_item*) linked_list_iter_next(&iter);
        task_free_ext_save_data(item);
        linked_list_iter_remove(&iter);
    }
}

Handle task_populate_ext_save_data(linked_list* items) {
    if(items == NULL) {
        return 0;
    }

    task_clear_ext_save_data(items);

    populate_ext_save_data_data* data = (populate_ext_save_data_data*) calloc(1, sizeof(populate_ext_save_data_data));
    if(data == NULL) {
        error_display(NULL, NULL, NULL, "Failed to allocate ext save data list data.");

        return 0;
    }

    data->items = items;

    Result eventRes = svcCreateEvent(&data->cancelEvent, 1);
    if(R_FAILED(eventRes)) {
        error_display_res(NULL, NULL, NULL, eventRes, "Failed to create ext save data list cancel event.");

        free(data);
        return 0;
    }

    if(threadCreate(task_populate_ext_save_data_thread, data, 0x10000, 0x18, 1, true) == NULL) {
        error_display(NULL, NULL, NULL, "Failed to create ext save data list thread.");

        svcCloseHandle(data->cancelEvent);
        free(data);
        return 0;
    }

    return data->cancelEvent;
}