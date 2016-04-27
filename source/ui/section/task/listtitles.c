#include <sys/syslimits.h>
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

typedef struct {
    linked_list* items;

    Handle cancelEvent;
} populate_titles_data;

static Result task_populate_titles_add_ctr(populate_titles_data* data, FS_MediaType mediaType, u64 titleId) {
    Result res = 0;

    AM_TitleEntry entry;
    if(R_SUCCEEDED(res = AM_GetTitleInfo(mediaType, 1, &titleId, &entry))) {
        list_item* item = (list_item*) calloc(1, sizeof(list_item));
        if(item != NULL) {
            title_info* titleInfo = (title_info*) calloc(1, sizeof(title_info));
            if(titleInfo != NULL) {
                titleInfo->mediaType = mediaType;
                titleInfo->titleId = titleId;
                AM_GetTitleProductCode(mediaType, titleId, titleInfo->productCode);
                titleInfo->version = entry.version;
                titleInfo->installedSize = entry.size;
                titleInfo->twl = false;
                titleInfo->hasMeta = false;

                static const u32 filePathData[] = {0x00000000, 0x00000000, 0x00000002, 0x6E6F6369, 0x00000000};
                static const FS_Path filePath = (FS_Path) {PATH_BINARY, 0x14, (u8*) filePathData};
                u32 archivePath[] = {(u32) (titleId & 0xFFFFFFFF), (u32) ((titleId >> 32) & 0xFFFFFFFF), mediaType, 0x00000000};
                FS_Archive archive = {ARCHIVE_SAVEDATA_AND_CONTENT, (FS_Path) {PATH_BINARY, 0x10, (u8*) archivePath}};

                Handle fileHandle;
                if(R_SUCCEEDED(FSUSER_OpenFileDirectly(&fileHandle, archive, filePath, FS_OPEN_READ, 0))) {
                    SMDH smdh;

                    u32 bytesRead = 0;
                    if(R_SUCCEEDED(FSFILE_Read(fileHandle, &bytesRead, 0, &smdh, sizeof(SMDH))) && bytesRead == sizeof(SMDH)) {
                        if(smdh.magic[0] == 'S' && smdh.magic[1] == 'M' && smdh.magic[2] == 'D' && smdh.magic[3] == 'H') {
                            titleInfo->hasMeta = true;

                            u8 systemLanguage = CFG_LANGUAGE_EN;
                            CFGU_GetSystemLanguage(&systemLanguage);

                            utf16_to_utf8((uint8_t*) item->name, smdh.titles[systemLanguage].shortDescription, NAME_MAX - 1);

                            utf16_to_utf8((uint8_t*) titleInfo->meta.shortDescription, smdh.titles[systemLanguage].shortDescription, sizeof(titleInfo->meta.shortDescription) - 1);
                            utf16_to_utf8((uint8_t*) titleInfo->meta.longDescription, smdh.titles[systemLanguage].longDescription, sizeof(titleInfo->meta.longDescription) - 1);
                            utf16_to_utf8((uint8_t*) titleInfo->meta.publisher, smdh.titles[systemLanguage].publisher, sizeof(titleInfo->meta.publisher) - 1);
                            titleInfo->meta.texture = screen_load_texture_tiled_auto(smdh.largeIcon, sizeof(smdh.largeIcon), 48, 48, GPU_RGB565, false);
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
                    snprintf(item->name, NAME_MAX, "%016llX", titleId);
                }

                if(mediaType == MEDIATYPE_NAND) {
                    item->color = COLOR_NAND;
                } else if(mediaType == MEDIATYPE_SD) {
                    item->color = COLOR_SD;
                } else if(mediaType == MEDIATYPE_GAME_CARD) {
                    item->color = COLOR_GAME_CARD;
                }

                item->data = titleInfo;

                linked_list_add(data->items, item);
            } else {
                free(item);

                res = R_FBI_OUT_OF_MEMORY;
            }
        } else {
            res = R_FBI_OUT_OF_MEMORY;
        }
    }

    return res;
}

static Result task_populate_titles_add_twl(populate_titles_data* data, FS_MediaType mediaType, u64 titleId) {
    Result res = 0;

    u64 realTitleId = 0;
    char productCode[12] = {'\0'};
    u16 version = 0;
    u64 installedSize = 0;

    AM_TitleEntry entry;
    if(R_SUCCEEDED(res = AM_GetTitleInfo(mediaType, 1, &titleId, &entry))) {
        realTitleId = titleId;
        AM_GetTitleProductCode(mediaType, titleId, productCode);
        version = entry.version;
        installedSize = entry.size;
    } else {
        u8 header[0x3B4] = {0};
        if(R_SUCCEEDED(res = FSUSER_GetLegacyRomHeader(mediaType, titleId, header))) {
            memcpy(&realTitleId, &header[0x230], sizeof(u64));
            memcpy(productCode, header, 0x00C);
            version = header[0x01E];

            u32 size = 0;
            if((header[0x012] & 0x2) != 0) {
                memcpy(&size, &header[0x210], sizeof(u32));
            } else {
                memcpy(&size, &header[0x080], sizeof(u32));
            }

            installedSize = size;
        }
    }

    if(R_SUCCEEDED(res)) {
        list_item* item = (list_item*) calloc(1, sizeof(list_item));
        if(item != NULL) {
            title_info* titleInfo = (title_info*) calloc(1, sizeof(title_info));
            if(titleInfo != NULL) {
                titleInfo->mediaType = mediaType;
                titleInfo->titleId = realTitleId;
                strncpy(titleInfo->productCode, productCode, 12);
                titleInfo->version = version;
                titleInfo->installedSize = installedSize;
                titleInfo->twl = true;
                titleInfo->hasMeta = false;

                BNR bnr;
                if(R_SUCCEEDED(FSUSER_GetLegacyBannerData(mediaType, titleId, (u8*) &bnr))) {
                    titleInfo->hasMeta = true;

                    u8 systemLanguage = CFG_LANGUAGE_EN;
                    CFGU_GetSystemLanguage(&systemLanguage);

                    char title[0x100] = {'\0'};
                    utf16_to_utf8((uint8_t*) title, bnr.titles[systemLanguage], sizeof(title) - 1);

                    if(strchr(title, '\n') == NULL) {
                        size_t len = strlen(title);
                        strncpy(item->name, title, len);
                        strncpy(titleInfo->meta.shortDescription, title, len);
                    } else {
                        char* destinations[] = {titleInfo->meta.shortDescription, titleInfo->meta.longDescription, titleInfo->meta.publisher};
                        int currDest = 0;

                        char* last = title;
                        char* curr = NULL;

                        while(currDest < 3 && (curr = strchr(last, '\n')) != NULL) {
                            strncpy(destinations[currDest++], last, curr - last);
                            last = curr + 1;
                            *curr = ' ';
                        }

                        strncpy(item->name, title, last - title);
                        if(currDest < 3) {
                            strncpy(destinations[currDest], last, strlen(title) - (last - title));
                        }
                    }

                    u8 icon[32 * 32 * 2];
                    for(u32 x = 0; x < 32; x++) {
                        for(u32 y = 0; y < 32; y++) {
                            u32 srcPos = (((y >> 3) * 4 + (x >> 3)) * 8 + (y & 7)) * 4 + ((x & 7) >> 1);
                            u32 srcShift = (x & 1) * 4;
                            u16 srcPx = bnr.mainIconPalette[(bnr.mainIconBitmap[srcPos] >> srcShift) & 0xF];

                            u8 r = (u8) (srcPx & 0x1F);
                            u8 g = (u8) ((srcPx >> 5) & 0x1F);
                            u8 b = (u8) ((srcPx >> 10) & 0x1F);

                            u16 reversedPx = (u16) ((r << 11) | (g << 6) | (b << 1) | 1);

                            u32 dstPos = (y * 32 + x) * 2;
                            icon[dstPos + 0] = (u8) (reversedPx & 0xFF);
                            icon[dstPos + 1] = (u8) ((reversedPx >> 8) & 0xFF);
                        }
                    }

                    titleInfo->meta.texture = screen_load_texture_auto(icon, sizeof(icon), 32, 32, GPU_RGBA5551, false);
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
                    snprintf(item->name, NAME_MAX, "%016llX", realTitleId);
                }

                item->color = COLOR_DS_TITLE;
                item->data = titleInfo;

                linked_list_add(data->items, item);
            } else {
                free(item);

                res = R_FBI_OUT_OF_MEMORY;
            }
        } else {
            res = R_FBI_OUT_OF_MEMORY;
        }
    }

    return res;
}

static Result task_populate_titles_from(populate_titles_data* data, FS_MediaType mediaType, bool useDSiWare) {
    bool inserted;
    FS_CardType type;
    if(mediaType == MEDIATYPE_GAME_CARD && (R_FAILED(FSUSER_CardSlotIsInserted(&inserted)) || !inserted || R_FAILED(FSUSER_GetCardType(&type)))) {
        return 0;
    }

    Result res = 0;

    if(mediaType != MEDIATYPE_GAME_CARD || type == CARD_CTR) {
        u32 titleCount = 0;
        if(R_SUCCEEDED(res = AM_GetTitleCount(mediaType, &titleCount))) {
            u64* titleIds = (u64*) calloc(titleCount, sizeof(u64));
            if(titleIds != NULL) {
                if(R_SUCCEEDED(res = AM_GetTitleList(&titleCount, mediaType, titleCount, titleIds))) {
                    qsort(titleIds, titleCount, sizeof(u64), util_compare_u64);

                    for(u32 i = 0; i < titleCount && R_SUCCEEDED(res); i++) {
                        svcWaitSynchronization(task_get_pause_event(), U64_MAX);
                        if(task_is_quit_all() || svcWaitSynchronization(data->cancelEvent, 0) == 0) {
                            break;
                        }

                        bool dsiWare = ((titleIds[i] >> 32) & 0x8000) != 0;
                        if(dsiWare != useDSiWare) {
                            continue;
                        }

                        res = dsiWare ? task_populate_titles_add_twl(data, mediaType, titleIds[i]) : task_populate_titles_add_ctr(data, mediaType, titleIds[i]);
                    }
                }

                free(titleIds);
            } else {
                res = R_FBI_OUT_OF_MEMORY;
            }
        }
    } else {
        res = task_populate_titles_add_twl(data, mediaType, 0);
    }

    return res;
}

static void task_populate_titles_thread(void* arg) {
    populate_titles_data* data = (populate_titles_data*) arg;

    Result res = 0;
    if(R_FAILED(res = task_populate_titles_from(data, MEDIATYPE_GAME_CARD, false)) || R_FAILED(res = task_populate_titles_from(data, MEDIATYPE_SD, false)) || R_FAILED(res = task_populate_titles_from(data, MEDIATYPE_NAND, false)) || R_FAILED(res = task_populate_titles_from(data, MEDIATYPE_NAND, true))) {
        error_display_res(NULL, NULL, NULL, res, "Failed to load title listing.");
    }

    svcCloseHandle(data->cancelEvent);
    free(data);
}

void task_free_title(list_item* item) {
    if(item == NULL) {
        return;
    }

    if(item->data != NULL) {
        title_info* titleInfo = (title_info*) item->data;
        if(titleInfo->hasMeta) {
            screen_unload_texture(titleInfo->meta.texture);
        }

        free(item->data);
    }

    free(item);
}

void task_clear_titles(linked_list* items) {
    if(items == NULL) {
        return;
    }

    linked_list_iter iter;
    linked_list_iterate(items, &iter);

    while(linked_list_iter_has_next(&iter)) {
        list_item* item = (list_item*) linked_list_iter_next(&iter);
        task_free_title(item);
        linked_list_iter_remove(&iter);
    }
}

Handle task_populate_titles(linked_list* items) {
    if(items == NULL) {
        return 0;
    }

    task_clear_titles(items);

    populate_titles_data* data = (populate_titles_data*) calloc(1, sizeof(populate_titles_data));
    if(data == NULL) {
        error_display(NULL, NULL, NULL, "Failed to allocate title list data.");

        return 0;
    }

    data->items = items;

    Result eventRes = svcCreateEvent(&data->cancelEvent, 1);
    if(R_FAILED(eventRes)) {
        error_display_res(NULL, NULL, NULL, eventRes, "Failed to create title list cancel event.");

        free(data);
        return 0;
    }

    if(threadCreate(task_populate_titles_thread, data, 0x10000, 0x18, 1, true) == NULL) {
        error_display(NULL, NULL, NULL, "Failed to create title list thread.");

        svcCloseHandle(data->cancelEvent);
        free(data);
        return 0;
    }

    return data->cancelEvent;
}