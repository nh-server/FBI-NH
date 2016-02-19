#include <sys/syslimits.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <3ds.h>

#include "../../list.h"
#include "../../error.h"
#include "../../../util.h"
#include "../../../screen.h"
#include "task.h"

typedef struct {
    list_item* items;
    u32* count;
    u32 max;

    Handle cancelEvent;
} populate_titles_data;

static Result task_populate_titles_from(populate_titles_data* data, FS_MediaType mediaType) {
    if(mediaType == MEDIATYPE_GAME_CARD && R_FAILED(FSUSER_GetCardType(NULL))) {
        return 0;
    }

    Result res = 0;

    u32 titleCount = 0;
    if(R_SUCCEEDED(res = AM_GetTitleCount(mediaType, &titleCount))) {
        u64* titleIds = (u64*) calloc(titleCount, sizeof(u64));
        if(titleIds != NULL) {
            if(R_SUCCEEDED(res = AM_GetTitleList(&titleCount, mediaType, titleCount, titleIds))) {
                qsort(titleIds, titleCount, sizeof(u64), util_compare_u64);

                AM_TitleEntry* titleInfos = (AM_TitleEntry*) calloc(titleCount, sizeof(AM_TitleEntry));
                if(titleInfos != NULL) {
                    if(R_SUCCEEDED(res = AM_GetTitleInfo(mediaType, titleCount, titleIds, titleInfos))) {
                        SMDH smdh;
                        for(u32 i = 0; i < titleCount && i < data->max; i++) {
                            if(task_is_quit_all() || svcWaitSynchronization(data->cancelEvent, 0) == 0) {
                                break;
                            }

                            title_info* titleInfo = (title_info*) calloc(1, sizeof(title_info));
                            if(titleInfo != NULL) {
                                titleInfo->mediaType = mediaType;
                                titleInfo->titleId = titleIds[i];
                                AM_GetTitleProductCode(mediaType, titleIds[i], titleInfo->productCode);
                                titleInfo->version = titleInfos[i].version;
                                titleInfo->installedSize = titleInfos[i].size;
                                titleInfo->hasSmdh = false;

                                list_item* item = &data->items[*data->count];

                                static const u32 filePathData[] = {0x00000000, 0x00000000, 0x00000002, 0x6E6F6369, 0x00000000};
                                static const FS_Path filePath = (FS_Path) {PATH_BINARY, 0x14, (u8*) filePathData};
                                u32 archivePath[] = {(u32) (titleIds[i] & 0xFFFFFFFF), (u32) ((titleIds[i] >> 32) & 0xFFFFFFFF), mediaType, 0x00000000};
                                FS_Archive archive = {ARCHIVE_SAVEDATA_AND_CONTENT, (FS_Path) {PATH_BINARY, 0x10, (u8*) archivePath}};
                                Handle fileHandle;
                                if(R_SUCCEEDED(FSUSER_OpenFileDirectly(&fileHandle, archive, filePath, FS_OPEN_READ, 0))) {
                                    u32 bytesRead;
                                    if(R_SUCCEEDED(FSFILE_Read(fileHandle, &bytesRead, 0, &smdh, sizeof(SMDH))) && bytesRead == sizeof(SMDH)) {
                                        if(smdh.magic[0] == 'S' && smdh.magic[1] == 'M' && smdh.magic[2] == 'D' && smdh.magic[3] == 'H') {
                                            u8 systemLanguage = CFG_LANGUAGE_EN;
                                            CFGU_GetSystemLanguage(&systemLanguage);

                                            utf16_to_utf8((uint8_t*) item->name, smdh.titles[systemLanguage].shortDescription, NAME_MAX - 1);

                                            titleInfo->hasSmdh = true;
                                            utf16_to_utf8((uint8_t*) titleInfo->smdhInfo.shortDescription, smdh.titles[systemLanguage].shortDescription, sizeof(titleInfo->smdhInfo.shortDescription) - 1);
                                            utf16_to_utf8((uint8_t*) titleInfo->smdhInfo.longDescription, smdh.titles[systemLanguage].longDescription, sizeof(titleInfo->smdhInfo.longDescription) - 1);
                                            utf16_to_utf8((uint8_t*) titleInfo->smdhInfo.publisher, smdh.titles[systemLanguage].publisher, sizeof(titleInfo->smdhInfo.publisher) - 1);
                                            titleInfo->smdhInfo.texture = screen_load_texture_tiled_auto(smdh.largeIcon, sizeof(smdh.largeIcon), 48, 48, GPU_RGB565, false);
                                        }
                                    }

                                    FSFILE_Close(fileHandle);
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
                                    snprintf(item->name, NAME_MAX, "%016llX", titleIds[i]);
                                }

                                if(mediaType == MEDIATYPE_NAND) {
                                    item->rgba = 0xFF0000FF;
                                } else if(mediaType == MEDIATYPE_SD) {
                                    item->rgba = 0xFF00FF00;
                                } else if(mediaType == MEDIATYPE_GAME_CARD) {
                                    item->rgba = 0xFFFF0000;
                                }

                                item->data = titleInfo;

                                (*data->count)++;
                            }
                        }
                    }

                    free(titleInfos);
                } else {
                    res = MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, 254, RD_OUT_OF_MEMORY);
                }
            }

            free(titleIds);
        } else {
            res = MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, 254, RD_OUT_OF_MEMORY);
        }
    }

    return res;
}

static void task_populate_titles_thread(void* arg) {
    populate_titles_data* data = (populate_titles_data*) arg;

    Result res = 0;
    if(R_FAILED(res = task_populate_titles_from(data, MEDIATYPE_GAME_CARD)) || R_FAILED(res = task_populate_titles_from(data, MEDIATYPE_SD)) || R_FAILED(res = task_populate_titles_from(data, MEDIATYPE_NAND))) {
        error_display_res(NULL, NULL, res, "Failed to load title listing.");
    }

    svcCloseHandle(data->cancelEvent);
    free(data);
}

static void task_clear_titles(list_item* items, u32* count) {
    if(items == NULL || count == NULL) {
        return;
    }

    u32 prevCount = *count;
    *count = 0;

    for(u32 i = 0; i < prevCount; i++) {
        if(items[i].data != NULL) {
            title_info* titleInfo = (title_info*) items[i].data;
            if(titleInfo->hasSmdh) {
                screen_unload_texture(titleInfo->smdhInfo.texture);
            }

            free(items[i].data);
            items[i].data = NULL;
        }

        memset(items[i].name, '\0', NAME_MAX);
        items[i].rgba = 0;
    }
}

Handle task_populate_titles(list_item* items, u32* count, u32 max) {
    if(items == NULL || count == NULL || max == 0) {
        return 0;
    }

    task_clear_titles(items, count);

    populate_titles_data* data = (populate_titles_data*) calloc(1, sizeof(populate_titles_data));
    data->items = items;
    data->count = count;
    data->max = max;

    Result eventRes = svcCreateEvent(&data->cancelEvent, 1);
    if(R_FAILED(eventRes)) {
        error_display_res(NULL, NULL, eventRes, "Failed to create title list cancel event.");

        free(data);
        return 0;
    }

    if(threadCreate(task_populate_titles_thread, data, 0x4000, 0x18, 1, true) == NULL) {
        error_display(NULL, NULL, "Failed to create title list thread.");

        svcCloseHandle(data->cancelEvent);
        free(data);
        return 0;
    }

    return data->cancelEvent;
}