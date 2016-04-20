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

                    AM_TitleEntry* titleInfos = (AM_TitleEntry*) calloc(titleCount, sizeof(AM_TitleEntry));
                    if(titleInfos != NULL) {
                        if(R_SUCCEEDED(res = AM_GetTitleInfo(mediaType, titleCount, titleIds, titleInfos))) {
                            SMDH* smdh = (SMDH*) calloc(1, sizeof(SMDH));
                            BNR* bnr = (BNR*) calloc(1, sizeof(BNR));
                            for(u32 i = 0; i < titleCount && i < data->max; i++) {
                                if(task_is_quit_all() || svcWaitSynchronization(data->cancelEvent, 0) == 0) {
                                    break;
                                }

                                bool dsiWare = ((titleIds[i] >> 32) & 0x8000) != 0;
                                if(dsiWare != useDSiWare) {
                                    continue;
                                }

                                title_info* titleInfo = (title_info*) calloc(1, sizeof(title_info));
                                if(titleInfo != NULL) {
                                    titleInfo->mediaType = mediaType;
                                    titleInfo->titleId = titleIds[i];
                                    AM_GetTitleProductCode(mediaType, titleIds[i], titleInfo->productCode);
                                    titleInfo->version = titleInfos[i].version;
                                    titleInfo->installedSize = titleInfos[i].size;
                                    titleInfo->twl = dsiWare;
                                    titleInfo->hasSmdh = false;

                                    list_item* item = &data->items[*data->count];

                                    if(dsiWare) {
                                        if(R_SUCCEEDED(FSUSER_GetLegacyBannerData(mediaType, titleIds[i], (u8*) bnr))) {
                                            titleInfo->hasSmdh = true;

                                            u8 systemLanguage = CFG_LANGUAGE_EN;
                                            CFGU_GetSystemLanguage(&systemLanguage);

                                            char title[0x100] = {'\0'};
                                            utf16_to_utf8((uint8_t*) title, bnr->titles[systemLanguage], 0x100);

                                            if(strchr(title, '\n') == NULL) {
                                                size_t len = strlen(title);
                                                strncpy(item->name, title, len);
                                                strncpy(titleInfo->smdhInfo.shortDescription, title, len);
                                            } else {
                                                char* destinations[] = {titleInfo->smdhInfo.shortDescription, titleInfo->smdhInfo.longDescription, titleInfo->smdhInfo.publisher};
                                                int currDest = 0;

                                                char* last = title;
                                                char* curr = NULL;

                                                while(currDest < 3 && (curr = strchr(last, '\n')) != NULL) {
                                                    if(currDest == 0) {
                                                        strncpy(item->name, last, curr - last);
                                                    }

                                                    strncpy(destinations[currDest++], last, curr - last);
                                                    last = curr + 1;
                                                }

                                                if(currDest < 3) {
                                                    strncpy(destinations[currDest], last, strlen(title) - (last - title));
                                                }

                                                strncpy(item->name, title, last - title);
                                                int len = strlen(item->name);
                                                for(int pos = 0; pos < len; pos++) {
                                                    if(item->name[pos] == '\n') {
                                                        item->name[pos] = ' ';
                                                    }
                                                }
                                            }

                                            u8 icon[32 * 32 * 2];
                                            for(u32 x = 0; x < 32; x++) {
                                                for(u32 y = 0; y < 32; y++) {
                                                    u32 srcPos = (((y >> 3) * 4 + (x >> 3)) * 8 + (y & 7)) * 4 + ((x & 7) >> 1);
                                                    u32 srcShift = (x & 1) * 4;
                                                    u16 srcPx = bnr->mainIconPalette[(bnr->mainIconBitmap[srcPos] >> srcShift) & 0xF];

                                                    u8 r = (u8) (srcPx & 0x1F);
                                                    u8 g = (u8) ((srcPx >> 5) & 0x1F);
                                                    u8 b = (u8) ((srcPx >> 10) & 0x1F);

                                                    u16 reversedPx = (u16) ((r << 11) | (g << 6) | (b << 1) | 1);

                                                    u32 dstPos = (y * 32 + x) * 2;
                                                    icon[dstPos + 0] = (u8) (reversedPx & 0xFF);
                                                    icon[dstPos + 1] = (u8) ((reversedPx >> 8) & 0xFF);
                                                }
                                            }

                                            titleInfo->smdhInfo.texture = screen_load_texture_auto(icon, sizeof(icon), 32, 32, GPU_RGBA5551, false);
                                        }
                                    } else {
                                        static const u32 filePathData[] = {0x00000000, 0x00000000, 0x00000002, 0x6E6F6369, 0x00000000};
                                        static const FS_Path filePath = (FS_Path) {PATH_BINARY, 0x14, (u8*) filePathData};
                                        u32 archivePath[] = {(u32) (titleIds[i] & 0xFFFFFFFF), (u32) ((titleIds[i] >> 32) & 0xFFFFFFFF), mediaType, 0x00000000};
                                        FS_Archive archive = {ARCHIVE_SAVEDATA_AND_CONTENT, (FS_Path) {PATH_BINARY, 0x10, (u8*) archivePath}};
                                        Handle fileHandle;
                                        if(R_SUCCEEDED(FSUSER_OpenFileDirectly(&fileHandle, archive, filePath, FS_OPEN_READ, 0))) {
                                            u32 bytesRead;
                                            if(R_SUCCEEDED(FSFILE_Read(fileHandle, &bytesRead, 0, smdh, sizeof(SMDH))) && bytesRead == sizeof(SMDH)) {
                                                if(smdh->magic[0] == 'S' && smdh->magic[1] == 'M' && smdh->magic[2] == 'D' && smdh->magic[3] == 'H') {
                                                    u8 systemLanguage = CFG_LANGUAGE_EN;
                                                    CFGU_GetSystemLanguage(&systemLanguage);

                                                    utf16_to_utf8((uint8_t*) item->name, smdh->titles[systemLanguage].shortDescription, NAME_MAX);

                                                    titleInfo->hasSmdh = true;
                                                    utf16_to_utf8((uint8_t*) titleInfo->smdhInfo.shortDescription, smdh->titles[systemLanguage].shortDescription, sizeof(titleInfo->smdhInfo.shortDescription));
                                                    utf16_to_utf8((uint8_t*) titleInfo->smdhInfo.longDescription, smdh->titles[systemLanguage].longDescription, sizeof(titleInfo->smdhInfo.longDescription));
                                                    utf16_to_utf8((uint8_t*) titleInfo->smdhInfo.publisher, smdh->titles[systemLanguage].publisher, sizeof(titleInfo->smdhInfo.publisher));
                                                    titleInfo->smdhInfo.texture = screen_load_texture_tiled_auto(smdh->largeIcon, sizeof(smdh->largeIcon), 48, 48, GPU_RGB565, false);
                                                }
                                            }

                                            FSFILE_Close(fileHandle);
                                        }
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
                                        if(dsiWare) {
                                            item->rgba = COLOR_DS_TITLE;
                                        } else {
                                            item->rgba = COLOR_NAND;
                                        }
                                    } else if(mediaType == MEDIATYPE_SD) {
                                        item->rgba = COLOR_SD;
                                    } else if(mediaType == MEDIATYPE_GAME_CARD) {
                                        item->rgba = COLOR_GAME_CARD;
                                    }

                                    item->data = titleInfo;

                                    (*data->count)++;
                                }
                            }

                            free(smdh);
                            free(bnr);
                        }

                        free(titleInfos);
                    } else {
                        res = R_FBI_OUT_OF_MEMORY;
                    }
                }

                free(titleIds);
            } else {
                res = R_FBI_OUT_OF_MEMORY;
            }
        }
    } else {
        u8* header = (u8*) calloc(1, 0x3B4);
        BNR* bnr = (BNR*) calloc(1, sizeof(BNR));

        if(R_SUCCEEDED(res = FSUSER_GetLegacyRomHeader(MEDIATYPE_GAME_CARD, 0, header)) && R_SUCCEEDED(res = FSUSER_GetLegacyBannerData(MEDIATYPE_GAME_CARD, 0, (u8*) bnr))) {
            title_info* titleInfo = (title_info*) calloc(1, sizeof(title_info));
            if(titleInfo != NULL) {
                titleInfo->mediaType = MEDIATYPE_GAME_CARD;
                titleInfo->titleId = *(u64*) &header[0x230];
                memcpy(titleInfo->productCode, header, 0x00C);
                titleInfo->version = header[0x01E];
                titleInfo->installedSize = titleInfo->titleId != 0 ? *(u32*) &header[0x210] : *(u32*) &header[0x080];
                titleInfo->twl = true;
                titleInfo->hasSmdh = true;

                list_item* item = &data->items[*data->count];

                u8 systemLanguage = CFG_LANGUAGE_EN;
                CFGU_GetSystemLanguage(&systemLanguage);

                char title[0x100] = {'\0'};
                utf16_to_utf8((uint8_t*) title, bnr->titles[systemLanguage], 0x100);

                if(strchr(title, '\n') == NULL) {
                    size_t len = strlen(title);
                    strncpy(item->name, title, len);
                    strncpy(titleInfo->smdhInfo.shortDescription, title, len);
                } else {
                    char* destinations[] = {titleInfo->smdhInfo.shortDescription, titleInfo->smdhInfo.longDescription, titleInfo->smdhInfo.publisher};
                    int currDest = 0;

                    char* last = title;
                    char* curr = NULL;

                    while(currDest < 3 && (curr = strchr(last, '\n')) != NULL) {
                        if(currDest == 0) {
                            strncpy(item->name, last, curr - last);
                        }

                        strncpy(destinations[currDest++], last, curr - last);
                        last = curr + 1;
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
                        u16 srcPx = bnr->mainIconPalette[(bnr->mainIconBitmap[srcPos] >> srcShift) & 0xF];

                        u8 r = (u8) (srcPx & 0x1F);
                        u8 g = (u8) ((srcPx >> 5) & 0x1F);
                        u8 b = (u8) ((srcPx >> 10) & 0x1F);

                        u16 reversedPx = (u16) ((r << 11) | (g << 6) | (b << 1) | 1);

                        u32 dstPos = (y * 32 + x) * 2;
                        icon[dstPos + 0] = (u8) (reversedPx & 0xFF);
                        icon[dstPos + 1] = (u8) ((reversedPx >> 8) & 0xFF);
                    }
                }

                titleInfo->smdhInfo.texture = screen_load_texture_auto(icon, sizeof(icon), 32, 32, GPU_RGBA5551, false);

                item->rgba = COLOR_DS_TITLE;
                item->data = titleInfo;

                (*data->count)++;
            }
        }

        free(header);
        free(bnr);
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
        error_display_res(NULL, NULL, NULL, eventRes, "Failed to create title list cancel event.");

        free(data);
        return 0;
    }

    if(threadCreate(task_populate_titles_thread, data, 0x4000, 0x18, 1, true) == NULL) {
        error_display(NULL, NULL, NULL, "Failed to create title list thread.");

        svcCloseHandle(data->cancelEvent);
        free(data);
        return 0;
    }

    return data->cancelEvent;
}