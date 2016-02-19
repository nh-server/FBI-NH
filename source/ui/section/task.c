#include <dirent.h>
#include <errno.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <3ds.h>

#include "task.h"
#include "../error.h"
#include "../../screen.h"
#include "../../util.h"
#include "../list.h"

#define EVENT_QUIT 0
#define EVENT_REFRESH_FILES 1
#define EVENT_REFRESH_TITLES 2
#define EVENT_REFRESH_PENDING_TITLES 3
#define EVENT_REFRESH_TICKETS 4
#define EVENT_REFRESH_EXT_SAVE_DATA 5
#define EVENT_REFRESH_SYSTEM_SAVE_DATA 6
#define EVENT_INSTALL_CIA 7
#define EVENT_COUNT 8

#define MAX_ENTRIES 1024

typedef struct {
    u16 shortDescription[0x40];
    u16 longDescription[0x80];
    u16 publisher[0x40];
} SMDH_title;

typedef struct {
    char magic[0x04];
    u16 version;
    u16 reserved1;
    SMDH_title titles[0x10];
    u8 ratings[0x10];
    u32 region;
    u32 matchMakerId;
    u64 matchMakerBitId;
    u32 flags;
    u16 eulaVersion;
    u16 reserved;
    u32 optimalBannerFrame;
    u32 streetpassId;
    u64 reserved2;
    u8 smallIcon[0x480];
    u8 largeIcon[0x1200];
} SMDH;

static char* files_path;
static FS_Archive* files_archive;

static bool files_init = false;
static list_item files_list[MAX_ENTRIES];
static u32 files_count;

static bool titles_init = false;
static list_item title_list[MAX_ENTRIES];
static u32 title_count = 0;

static bool pending_titles_init = false;
static list_item pending_title_list[MAX_ENTRIES];
static u32 pending_title_count = 0;

static bool tickets_init = false;
static list_item ticket_list[MAX_ENTRIES];
static u32 ticket_count = 0;

static bool ext_save_data_init = false;
static list_item ext_save_data_list[MAX_ENTRIES];
static u32 ext_save_data_count = 0;

static bool system_save_data_init = false;
static list_item system_save_data_list[MAX_ENTRIES];
static u32 system_save_data_count = 0;

static bool cia_installing;
static Result cia_result;
static int cia_errno;
static bool cia_cancelled;
static FS_MediaType cia_dest;
static u64 cia_size;
static void* cia_data;
static Result (*cia_read)(void* data, u32* bytesRead, void* buffer, u32 size);

static Handle events[EVENT_COUNT];
static Handle mutex;
static Thread task_thread_ptr;

static int sort_ids(const void* e1, const void* e2) {
    u64 id1 = *(u64*) e1;
    u64 id2 = *(u64*) e2;

    return id1 > id2 ? 1 : id1 < id2 ? -1 : 0;
}

static int sort_directory_entries(const void* e1, const void* e2) {
    FS_DirectoryEntry* ent1 = (FS_DirectoryEntry*) e1;
    FS_DirectoryEntry* ent2 = (FS_DirectoryEntry*) e2;

    if((ent1->attributes & FS_ATTRIBUTE_DIRECTORY) && !(ent2->attributes & FS_ATTRIBUTE_DIRECTORY)) {
        return -1;
    } else if(!(ent1->attributes & FS_ATTRIBUTE_DIRECTORY) && (ent2->attributes & FS_ATTRIBUTE_DIRECTORY)) {
        return 1;
    } else {
        char entryName1[0x213] = {'\0'};
        utf16_to_utf8((uint8_t*) entryName1, ent1->name, sizeof(entryName1) - 1);

        char entryName2[0x213] = {'\0'};
        utf16_to_utf8((uint8_t*) entryName2, ent2->name, sizeof(entryName2) - 1);

        return strcasecmp(entryName1, entryName2);
    }
}

static void task_clear_files() {
    svcWaitSynchronization(mutex, U64_MAX);
    files_count = 0;
    svcReleaseMutex(mutex);

    for(int i = 0; i < MAX_ENTRIES; i++) {
        if(files_list[i].data != NULL) {
            file_info* fileInfo = (file_info*) files_list[i].data;
            if(fileInfo->isCia && fileInfo->ciaInfo.hasSmdh) {
                screen_unload_texture(fileInfo->ciaInfo.smdhInfo.texture);
            }

            free(files_list[i].data);
            files_list[i].data = NULL;
        }

        memset(files_list[i].name, '\0', NAME_MAX);
        files_list[i].rgba = 0;
    }
}

static Result task_load_files() {
    if(files_archive == NULL || files_path == NULL) {
        return 0;
    }

    files_init = true;

    file_info* dotFileInfo = (file_info*) calloc(1, sizeof(file_info));
    if(dotFileInfo != NULL) {
        dotFileInfo->archive = files_archive;
        strncpy(dotFileInfo->path, files_path, PATH_MAX);
        util_get_path_file(dotFileInfo->name, dotFileInfo->path, NAME_MAX);
        dotFileInfo->isDirectory = true;
        dotFileInfo->containsCias = false;
        dotFileInfo->size = 0;
        dotFileInfo->isCia = false;

        list_item* dotItem = &files_list[files_count];
        strncpy(dotItem->name, ".", NAME_MAX);
        dotItem->rgba = 0xFF0000FF;
        dotItem->data = dotFileInfo;

        files_count++;
    }

    file_info* dotDotFileInfo = (file_info*) calloc(1, sizeof(file_info));
    if(dotDotFileInfo != NULL) {
        dotDotFileInfo->archive = files_archive;
        util_get_parent_path(dotDotFileInfo->path, files_path, PATH_MAX);
        util_get_path_file(dotDotFileInfo->name, dotDotFileInfo->path, NAME_MAX);
        dotDotFileInfo->isDirectory = true;
        dotDotFileInfo->containsCias = false;
        dotDotFileInfo->size = 0;
        dotDotFileInfo->isCia = false;

        list_item* dotDotItem = &files_list[files_count];
        strncpy(dotDotItem->name, "..", NAME_MAX);
        dotDotItem->rgba = 0xFF0000FF;
        dotDotItem->data = dotDotFileInfo;

        files_count++;
    }

    Result res = 0;

    Handle dirHandle = 0;
    if(R_SUCCEEDED(res = FSUSER_OpenDirectory(&dirHandle, *files_archive, fsMakePath(PATH_ASCII, files_path)))) {
        u32 entryCount = 0;
        FS_DirectoryEntry* entries = (FS_DirectoryEntry*) calloc(MAX_ENTRIES, sizeof(FS_DirectoryEntry));
        if(entries != NULL) {
            if(R_SUCCEEDED(res = FSDIR_Read(dirHandle, &entryCount, MAX_ENTRIES, entries)) && entryCount > 0) {
                qsort(entries, entryCount, sizeof(FS_DirectoryEntry), sort_directory_entries);

                SMDH smdh;
                for(u32 i = 0; i < entryCount && i < MAX_ENTRIES; i++) {
                    s32 index = 0;
                    if(svcWaitSynchronizationN(&index, events, EVENT_COUNT, false, 0) == 0) {
                        files_init = false;

                        svcSignalEvent(events[index]); // Pass on event signal.
                        break;
                    }

                    if(entries[i].attributes & FS_ATTRIBUTE_HIDDEN) {
                        continue;
                    }

                    file_info* fileInfo = (file_info*) calloc(1, sizeof(file_info));
                    if(fileInfo != NULL) {
                        char entryName[0x213] = {'\0'};
                        utf16_to_utf8((uint8_t*) entryName, entries[i].name, sizeof(entryName) - 1);

                        fileInfo->archive = files_archive;
                        strncpy(fileInfo->name, entryName, NAME_MAX);

                        list_item* item = &files_list[files_count];

                        if(entries[i].attributes & FS_ATTRIBUTE_DIRECTORY) {
                            item->rgba = 0xFF0000FF;

                            snprintf(fileInfo->path, PATH_MAX, "%s%s/", files_path, entryName);
                            fileInfo->isDirectory = true;
                            fileInfo->containsCias = false;
                            fileInfo->size = 0;
                            fileInfo->isCia = false;
                        } else {
                            item->rgba = 0xFF000000;

                            snprintf(fileInfo->path, PATH_MAX, "%s%s", files_path, entryName);
                            fileInfo->isDirectory = false;
                            fileInfo->containsCias = false;
                            fileInfo->size = 0;
                            fileInfo->isCia = false;

                            Handle fileHandle;
                            if(R_SUCCEEDED(FSUSER_OpenFile(&fileHandle, *files_archive, fsMakePath(PATH_ASCII, fileInfo->path), FS_OPEN_READ, 0))) {
                                FSFILE_GetSize(fileHandle, &fileInfo->size);

                                AM_TitleEntry titleEntry;
                                if(R_SUCCEEDED(AM_GetCiaFileInfo(MEDIATYPE_SD, &titleEntry, fileHandle))) {
                                    dotFileInfo->containsCias = true;

                                    fileInfo->isCia = true;
                                    fileInfo->ciaInfo.titleId = titleEntry.titleID;
                                    fileInfo->ciaInfo.version = titleEntry.version;
                                    fileInfo->ciaInfo.installedSizeSD = titleEntry.size;
                                    fileInfo->ciaInfo.hasSmdh = false;

                                    if(R_SUCCEEDED(AM_GetCiaFileInfo(MEDIATYPE_NAND, &titleEntry, fileHandle))) {
                                        fileInfo->ciaInfo.installedSizeNAND = titleEntry.size;
                                    } else {
                                        fileInfo->ciaInfo.installedSizeNAND = 0;
                                    }

                                    u32 bytesRead;
                                    if(R_SUCCEEDED(FSFILE_Read(fileHandle, &bytesRead, fileInfo->size - sizeof(SMDH), &smdh, sizeof(SMDH))) && bytesRead == sizeof(SMDH)) {
                                        if(smdh.magic[0] == 'S' && smdh.magic[1] == 'M' && smdh.magic[2] == 'D' && smdh.magic[3] == 'H') {
                                            u8 systemLanguage = CFG_LANGUAGE_EN;
                                            CFGU_GetSystemLanguage(&systemLanguage);

                                            fileInfo->ciaInfo.hasSmdh = true;
                                            utf16_to_utf8((uint8_t*) fileInfo->ciaInfo.smdhInfo.shortDescription, smdh.titles[systemLanguage].shortDescription, sizeof(fileInfo->ciaInfo.smdhInfo.shortDescription) - 1);
                                            utf16_to_utf8((uint8_t*) fileInfo->ciaInfo.smdhInfo.longDescription, smdh.titles[systemLanguage].longDescription, sizeof(fileInfo->ciaInfo.smdhInfo.longDescription) - 1);
                                            utf16_to_utf8((uint8_t*) fileInfo->ciaInfo.smdhInfo.publisher, smdh.titles[systemLanguage].publisher, sizeof(fileInfo->ciaInfo.smdhInfo.publisher) - 1);
                                            fileInfo->ciaInfo.smdhInfo.texture = screen_load_texture_tiled_auto(smdh.largeIcon, sizeof(smdh.largeIcon), 48, 48, GPU_RGB565, false);
                                        }
                                    }
                                }

                                FSFILE_Close(fileHandle);
                            }
                        }

                        strncpy(item->name, entryName, NAME_MAX);
                        item->data = fileInfo;

                        svcWaitSynchronization(mutex, U64_MAX);
                        files_count++;
                        svcReleaseMutex(mutex);
                    }
                }
            }

            free(entries);
        } else {
            res = MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, 254, RD_OUT_OF_MEMORY);
        }

        FSDIR_Close(dirHandle);
    }

    return res;
}

static void task_clear_titles() {
    svcWaitSynchronization(mutex, U64_MAX);
    title_count = 0;
    svcReleaseMutex(mutex);

    for(u32 i = 0; i < MAX_ENTRIES; i++) {
        if(title_list[i].data != NULL) {
            title_info* titleInfo = (title_info*) title_list[i].data;
            if(titleInfo->hasSmdh) {
                screen_unload_texture(titleInfo->smdhInfo.texture);
            }

            free(title_list[i].data);
            title_list[i].data = NULL;
        }

        memset(title_list[i].name, '\0', NAME_MAX);
        title_list[i].rgba = 0;
    }
}

static Result task_load_titles(FS_MediaType mediaType) {
    titles_init = true;

    if(mediaType == MEDIATYPE_GAME_CARD && R_FAILED(FSUSER_GetCardType(NULL))) {
        return 0;
    }

    Result res = 0;

    u32 titleCount = 0;
    if(R_SUCCEEDED(res = AM_GetTitleCount(mediaType, &titleCount))) {
        u64* titleIds = (u64*) calloc(titleCount, sizeof(u64));
        if(titleIds != NULL) {
            if(R_SUCCEEDED(res = AM_GetTitleList(&titleCount, mediaType, titleCount, titleIds))) {
                qsort(titleIds, titleCount, sizeof(u64), sort_ids);

                AM_TitleEntry* titleInfos = (AM_TitleEntry*) calloc(titleCount, sizeof(AM_TitleEntry));
                if(titleInfos != NULL) {
                    if(R_SUCCEEDED(res = AM_GetTitleInfo(mediaType, titleCount, titleIds, titleInfos))) {
                        SMDH smdh;
                        for(u32 i = 0; i < titleCount && i < MAX_ENTRIES; i++) {
                            s32 index = 0;
                            if(svcWaitSynchronizationN(&index, events, EVENT_COUNT, false, 0) == 0) {
                                titles_init = false;

                                svcSignalEvent(events[index]); // Pass on event signal.
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

                                list_item* item = &title_list[title_count];

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

                                svcWaitSynchronization(mutex, U64_MAX);
                                title_count++;
                                svcReleaseMutex(mutex);
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

static void task_clear_pending_titles() {
    svcWaitSynchronization(mutex, U64_MAX);
    pending_title_count = 0;
    svcReleaseMutex(mutex);

    for(u32 i = 0; i < MAX_ENTRIES; i++) {
        if(pending_title_list[i].data != NULL) {
            free(pending_title_list[i].data);
            pending_title_list[i].data = NULL;
        }

        memset(pending_title_list[i].name, '\0', NAME_MAX);
        pending_title_list[i].rgba = 0;
    }
}

static Result task_load_pending_titles(FS_MediaType mediaType) {
    pending_titles_init = true;

    Result res = 0;

    u32 pendingTitleCount = 0;
    if(R_SUCCEEDED(res = AM_GetPendingTitleCount(&pendingTitleCount, mediaType, AM_STATUS_MASK_INSTALLING | AM_STATUS_MASK_AWAITING_FINALIZATION))) {
        u64* pendingTitleIds = (u64*) calloc(pendingTitleCount, sizeof(u64));
        if(pendingTitleIds != NULL) {
            if(R_SUCCEEDED(res = AM_GetPendingTitleList(&pendingTitleCount, pendingTitleCount, mediaType, AM_STATUS_MASK_INSTALLING | AM_STATUS_MASK_AWAITING_FINALIZATION, pendingTitleIds))) {
                qsort(pendingTitleIds, pendingTitleCount, sizeof(u64), sort_ids);

                AM_PendingTitleEntry* pendingTitleInfos = (AM_PendingTitleEntry*) calloc(pendingTitleCount, sizeof(AM_PendingTitleEntry));
                if(pendingTitleInfos != NULL) {
                    if(R_SUCCEEDED(res = AM_GetPendingTitleInfo(pendingTitleCount, mediaType, pendingTitleIds, pendingTitleInfos))) {
                        for(u32 i = 0; i < pendingTitleCount && i < MAX_ENTRIES; i++) {
                            s32 index = 0;
                            if(svcWaitSynchronizationN(&index, events, EVENT_COUNT, false, 0) == 0) {
                                pending_titles_init = false;

                                svcSignalEvent(events[index]); // Pass on event signal.
                                break;
                            }

                            pending_title_info* pendingTitleInfo = (pending_title_info*) calloc(1, sizeof(pending_title_info));
                            if(pendingTitleInfo != NULL) {
                                pendingTitleInfo->mediaType = mediaType;
                                pendingTitleInfo->titleId = pendingTitleIds[i];
                                pendingTitleInfo->version = pendingTitleInfos[i].version;

                                list_item* item = &pending_title_list[pending_title_count];
                                snprintf(item->name, NAME_MAX, "%016llX", pendingTitleIds[i]);
                                if(mediaType == MEDIATYPE_NAND) {
                                    item->rgba = 0xFF0000FF;
                                } else if(mediaType == MEDIATYPE_SD) {
                                    item->rgba = 0xFF00FF00;
                                } else if(mediaType == MEDIATYPE_GAME_CARD) {
                                    item->rgba = 0xFFFF0000;
                                }

                                item->data = pendingTitleInfo;

                                svcWaitSynchronization(mutex, U64_MAX);
                                pending_title_count++;
                                svcReleaseMutex(mutex);
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

static void task_clear_tickets() {
    svcWaitSynchronization(mutex, U64_MAX);
    ticket_count = 0;
    svcReleaseMutex(mutex);

    for(u32 i = 0; i < MAX_ENTRIES; i++) {
        if(ticket_list[i].data != NULL) {
            free(ticket_list[i].data);
            ticket_list[i].data = NULL;
        }

        memset(ticket_list[i].name, '\0', NAME_MAX);
        ticket_list[i].rgba = 0;
    }
}

static Result task_load_tickets() {
    tickets_init = true;

    Result res = 0;

    u32 ticketCount = 0;
    if(R_SUCCEEDED(res = AM_GetTicketCount(&ticketCount))) {
        u64* ticketIds = (u64*) calloc(ticketCount, sizeof(u64));
        if(ticketIds != NULL) {
            if(R_SUCCEEDED(res = AM_GetTicketList(&ticketCount, ticketCount, 0, ticketIds))) {
                qsort(ticketIds, ticketCount, sizeof(u64), sort_ids);

                for(u32 i = 0; i < ticketCount && i < MAX_ENTRIES; i++) {
                    s32 index = 0;
                    if(svcWaitSynchronizationN(&index, events, EVENT_COUNT, false, 0) == 0) {
                        tickets_init = false;

                        svcSignalEvent(events[index]); // Pass on event signal.
                        break;
                    }

                    ticket_info* ticketInfo = (ticket_info*) calloc(1, sizeof(ticket_info));
                    if(ticketInfo != NULL) {
                        ticketInfo->ticketId = ticketIds[i];

                        list_item* item = &ticket_list[ticket_count];
                        snprintf(item->name, NAME_MAX, "%016llX", ticketIds[i]);
                        item->rgba = 0xFF000000;
                        item->data = ticketInfo;

                        svcWaitSynchronization(mutex, U64_MAX);
                        ticket_count++;
                        svcReleaseMutex(mutex);
                    }
                }
            }

            free(ticketIds);
        } else {
            res = MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, 254, RD_OUT_OF_MEMORY);
        }
    }

    return res;
}

static void task_clear_ext_save_data() {
    svcWaitSynchronization(mutex, U64_MAX);
    ext_save_data_count = 0;
    svcReleaseMutex(mutex);

    for(u32 i = 0; i < MAX_ENTRIES; i++) {
        if(ext_save_data_list[i].data != NULL) {
            ext_save_data_info* extSaveDataInfo = (ext_save_data_info*) ext_save_data_list[i].data;
            if(extSaveDataInfo->hasSmdh) {
                screen_unload_texture(extSaveDataInfo->smdhInfo.texture);
            }

            free(ext_save_data_list[i].data);
            ext_save_data_list[i].data = NULL;
        }

        memset(ext_save_data_list[i].name, '\0', NAME_MAX);
        ext_save_data_list[i].rgba = 0;
    }
}

static Result task_load_ext_save_data(FS_MediaType mediaType) {
    ext_save_data_init = true;

    Result res = 0;

    u32 extSaveDataCount = 0;
    u64* extSaveDataIds = (u64*) calloc(MAX_ENTRIES, sizeof(u64));
    if(extSaveDataIds != NULL) {
        if(R_SUCCEEDED(res = FSUSER_EnumerateExtSaveData(&extSaveDataCount, MAX_ENTRIES, mediaType, 8, mediaType == MEDIATYPE_NAND, (u8*) extSaveDataIds))) {
            qsort(extSaveDataIds, extSaveDataCount, sizeof(u64), sort_ids);

            SMDH smdh;
            for(u32 i = 0; i < extSaveDataCount && i < MAX_ENTRIES; i++) {
                s32 index = 0;
                if(svcWaitSynchronizationN(&index, events, EVENT_COUNT, false, 0) == 0) {
                    ext_save_data_init = false;

                    svcSignalEvent(events[index]); // Pass on event signal.
                    break;
                }

                ext_save_data_info* extSaveDataInfo = (ext_save_data_info*) calloc(1, sizeof(ext_save_data_info));
                if(extSaveDataInfo != NULL) {
                    extSaveDataInfo->mediaType = mediaType;
                    extSaveDataInfo->extSaveDataId = extSaveDataIds[i];
                    extSaveDataInfo->shared = mediaType == MEDIATYPE_NAND;

                    list_item* item = &ext_save_data_list[ext_save_data_count];

                    FS_ExtSaveDataInfo info = {.mediaType = mediaType, .saveId = extSaveDataIds[i]};
                    u32 smdhBytesRead = 0;
                    if(R_SUCCEEDED(FSUSER_ReadExtSaveDataIcon(&smdhBytesRead, info, sizeof(SMDH), (u8*) &smdh)) && smdhBytesRead == sizeof(SMDH)) {
                        u8 systemLanguage = CFG_LANGUAGE_EN;
                        CFGU_GetSystemLanguage(&systemLanguage);

                        utf16_to_utf8((uint8_t*) item->name, smdh.titles[systemLanguage].shortDescription, NAME_MAX - 1);

                        extSaveDataInfo->hasSmdh = true;
                        utf16_to_utf8((uint8_t*) extSaveDataInfo->smdhInfo.shortDescription, smdh.titles[systemLanguage].shortDescription, sizeof(extSaveDataInfo->smdhInfo.shortDescription) - 1);
                        utf16_to_utf8((uint8_t*) extSaveDataInfo->smdhInfo.longDescription, smdh.titles[systemLanguage].longDescription, sizeof(extSaveDataInfo->smdhInfo.longDescription) - 1);
                        utf16_to_utf8((uint8_t*) extSaveDataInfo->smdhInfo.publisher, smdh.titles[systemLanguage].publisher, sizeof(extSaveDataInfo->smdhInfo.publisher) - 1);
                        extSaveDataInfo->smdhInfo.texture = screen_load_texture_tiled_auto(smdh.largeIcon, sizeof(smdh.largeIcon), 48, 48, GPU_RGB565, false);
                    } else {
                        extSaveDataInfo->hasSmdh = false;
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
                        item->rgba = 0xFF0000FF;
                    } else if(mediaType == MEDIATYPE_SD) {
                        item->rgba = 0xFF00FF00;
                    } else if(mediaType == MEDIATYPE_GAME_CARD) {
                        item->rgba = 0xFFFF0000;
                    }

                    item->data = extSaveDataInfo;

                    svcWaitSynchronization(mutex, U64_MAX);
                    ext_save_data_count++;
                    svcReleaseMutex(mutex);
                }
            }
        }

        free(extSaveDataIds);
    } else {
        res = MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, 254, RD_OUT_OF_MEMORY);
    }

    return res;
}

static void task_clear_system_save_data() {
    svcWaitSynchronization(mutex, U64_MAX);
    system_save_data_count = 0;
    svcReleaseMutex(mutex);

    for(u32 i = 0; i < MAX_ENTRIES; i++) {
        if(system_save_data_list[i].data != NULL) {
            free(system_save_data_list[i].data);
            system_save_data_list[i].data = NULL;
        }

        memset(system_save_data_list[i].name, '\0', NAME_MAX);
        system_save_data_list[i].rgba = 0;
    }
}

static Result task_load_system_save_data() {
    system_save_data_init = true;

    Result res = 0;

    u32 systemSaveDataCount = 0;
    u64* systemSaveDataIds = (u64*) calloc(MAX_ENTRIES, sizeof(u64));
    if(systemSaveDataIds != NULL) {
        if(R_SUCCEEDED(res = FSUSER_EnumerateSystemSaveData(&systemSaveDataCount, MAX_ENTRIES * sizeof(u64), systemSaveDataIds))) {
            qsort(systemSaveDataIds, systemSaveDataCount, sizeof(u64), sort_ids);

            for(u32 i = 0; i < systemSaveDataCount && i < MAX_ENTRIES; i++) {
                s32 index = 0;
                if(svcWaitSynchronizationN(&index, events, EVENT_COUNT, false, 0) == 0) {
                    system_save_data_init = false;

                    svcSignalEvent(events[index]); // Pass on event signal.
                    break;
                }

                system_save_data_info* systemSaveDataInfo = (system_save_data_info*) calloc(1, sizeof(system_save_data_info));
                if(systemSaveDataInfo != NULL) {
                    systemSaveDataInfo->systemSaveDataId = systemSaveDataIds[i];

                    list_item* item = &system_save_data_list[system_save_data_count];
                    snprintf(item->name, NAME_MAX, "%016llX", systemSaveDataIds[i]);
                    item->rgba = 0xFF000000;
                    item->data = systemSaveDataInfo;

                    svcWaitSynchronization(mutex, U64_MAX);
                    system_save_data_count++;
                    svcReleaseMutex(mutex);
                }
            }
        }

        free(systemSaveDataIds);
    } else {
        res = MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, 254, RD_OUT_OF_MEMORY);
    }

    return res;
}

#define bswap_64(x) \
({ \
	uint64_t __x = (x); \
	((uint64_t)( \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0x00000000000000ffULL) << 56) | \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0x000000000000ff00ULL) << 40) | \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0x0000000000ff0000ULL) << 24) | \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0x00000000ff000000ULL) <<  8) | \
	    (uint64_t)(((uint64_t)(__x) & (uint64_t)0x000000ff00000000ULL) >>  8) | \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0x0000ff0000000000ULL) >> 24) | \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0x00ff000000000000ULL) >> 40) | \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0xff00000000000000ULL) >> 56) )); \
})

static u32 align(u32 offset, u32 alignment) {
    return (offset + (alignment - 1)) & ~(alignment - 1);
}

static void task_install_cia() {
    if(cia_read == NULL) {
        return;
    }

    Result res = 0;

    cia_result = 0;
    cia_errno = 0;
    cia_cancelled = false;
    cia_installing = true;

    u32 bufferSize = 1024 * 256;
    u8* buffer = (u8*) calloc(1, bufferSize);
    if(buffer != NULL) {
        bool firstBlock = true;
        u64 titleId = 0;

        Handle ciaHandle = 0;

        u32 bytesRead = 0;
        u32 bytesWritten = 0;
        u64 offset = 0;
        while(offset < cia_size) {
            s32 index = 0;
            if(svcWaitSynchronizationN(&index, events, EVENT_COUNT, false, 0) == 0) {
                cia_cancelled = true;
                svcSignalEvent(events[index]); // Pass on event signal.
            }

            if(cia_cancelled) {
                cia_cancelled = false;
                res = CIA_INSTALL_RESULT_CANCELLED;
                break;
            }

            u32 readSize = bufferSize;
            if(cia_size - offset < readSize) {
                readSize = (u32) (cia_size - offset);
            }

            if(R_FAILED(res = cia_read(cia_data, &bytesRead, buffer, readSize))) {
                if(res == CIA_INSTALL_RESULT_ERRNO) {
                    cia_errno = errno;
                }

                break;
            }

            if(firstBlock) {
                firstBlock = false;

                u32 headerSize = *(u32*) &buffer[0x00];
                u32 certSize = *(u32*) &buffer[0x08];
                titleId = bswap_64(*(u64*) &buffer[align(headerSize, 64) + align(certSize, 64) + 0x1DC]);

                if((titleId >> 32) & 0x8000) {
                    cia_dest = MEDIATYPE_NAND;
                }

                u8 n3ds = false;
                if(R_SUCCEEDED(APT_CheckNew3DS(&n3ds)) && !n3ds && ((titleId >> 28) & 0xF) == 2) {
                    res = CIA_INSTALL_RESULT_WRONG_SYSTEM;
                    break;
                }

                if(R_FAILED(res = AM_StartCiaInstall(cia_dest, &ciaHandle))) {
                    break;
                }
            }

            if(R_FAILED(res = FSFILE_Write(ciaHandle, &bytesWritten, offset, buffer, bytesRead, 0))) {
                break;
            }

            offset += bytesRead;
        }

        free(buffer);

        if(ciaHandle != 0) {
            if(R_FAILED(res)) {
                AM_CancelCIAInstall(ciaHandle);
            } else if(R_SUCCEEDED(res = AM_FinishCiaInstall(ciaHandle))) {
                if(titleId == 0x0004013800000002LL || titleId == 0x0004013820000002LL) {
                    res = AM_InstallFirm(titleId);
                }
            }
        }
    } else {
        res = MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, 254, RD_OUT_OF_MEMORY);
    }

    cia_cancelled = false;
    cia_result = res;
    cia_installing = false;
}

static void task_thread(void* arg) {
    while(true) {
        s32 index = 0;
        svcWaitSynchronizationN(&index, events, EVENT_COUNT, false, U64_MAX);
        if(index == EVENT_QUIT) {
            task_clear_files();
            task_clear_titles();
            task_clear_pending_titles();
            task_clear_tickets();

            break;
        } else if(index == EVENT_REFRESH_FILES) {
            task_clear_files();

            Result res = 0;
            if(R_FAILED(res = task_load_files())) {
                error_display_res(NULL, NULL, res, "Failed to load file listing.");
            }
        } else if(index == EVENT_REFRESH_TITLES) {
            task_clear_titles();

            Result res = 0;
            if(R_FAILED(res = task_load_titles(MEDIATYPE_GAME_CARD)) || R_FAILED(res = task_load_titles(MEDIATYPE_SD)) || R_FAILED(res = task_load_titles(MEDIATYPE_NAND))) {
                error_display_res(NULL, NULL, res, "Failed to load title listing.");
            }
        } else if(index == EVENT_REFRESH_PENDING_TITLES) {
            task_clear_pending_titles();

            Result res = 0;
            if(R_FAILED(res = task_load_pending_titles(MEDIATYPE_SD)) || R_FAILED(res = task_load_pending_titles(MEDIATYPE_NAND))) {
                error_display_res(NULL, NULL, res, "Failed to load pending title listing.");
            }
        } else if(index == EVENT_REFRESH_TICKETS) {
            task_clear_tickets();

            Result res = 0;
            if(R_FAILED(res = task_load_tickets())) {
                error_display_res(NULL, NULL, res, "Failed to load ticket listing.");
            }
        } else if(index == EVENT_REFRESH_EXT_SAVE_DATA) {
            task_clear_ext_save_data();

            Result res = 0;
            if(R_FAILED(res = task_load_ext_save_data(MEDIATYPE_SD)) || R_FAILED(res = task_load_ext_save_data(MEDIATYPE_NAND))) {
                error_display_res(NULL, NULL, res, "Failed to load ext save data listing.");
            }
        } else if(index == EVENT_REFRESH_SYSTEM_SAVE_DATA) {
            task_clear_system_save_data();

            Result res = 0;
            if(R_FAILED(res = task_load_system_save_data())) {
                error_display_res(NULL, NULL, res, "Failed to load system save data listing.");
            }
        } else if(index == EVENT_INSTALL_CIA) {
            task_install_cia();
        }
    }
}

void task_init() {
    aptOpenSession();
    Result setCpuTimeRes = APT_SetAppCpuTimeLimit(30);
    aptCloseSession();

    if(R_FAILED(setCpuTimeRes)) {
        util_panic("Failed to set syscore CPU time: %08lX", setCpuTimeRes);
        return;
    }

    for(int i = 0; i < EVENT_COUNT; i++) {
        Result eventRes = svcCreateEvent(&events[i], 0);
        if(R_FAILED(eventRes)) {
            util_panic("Failed to create thread event: %08lX", eventRes);
            return;
        }
    }

    Result mutexRes = svcCreateMutex(&mutex, false);
    if(R_FAILED(mutexRes)) {
        util_panic("Failed to create thread mutex: %08lX", mutexRes);
        return;
    }

    task_thread_ptr = threadCreate(task_thread, NULL, 0x10000, 0x18, 1, false);
    if(task_thread_ptr == NULL) {
        util_panic("Failed to create task thread.");
        return;
    }
}

void task_exit() {
    if(task_thread_ptr != NULL) {
        svcSignalEvent(events[EVENT_QUIT]);
        threadJoin(task_thread_ptr, U64_MAX);

        threadFree(task_thread_ptr);
        task_thread_ptr = NULL;
    }

    for(int i = 0; i < EVENT_COUNT; i++) {
        if(events[i] != 0) {
            svcCloseHandle(events[i]);
            events[i] = 0;
        }
    }

    if(mutex != 0) {
        svcReleaseMutex(mutex);
        svcCloseHandle(mutex);
        mutex = 0;
    }

    files_archive = NULL;
    files_path = NULL;

    files_init = false;
    titles_init = false;
    pending_titles_init = false;
    tickets_init = false;
    ext_save_data_init = false;
    system_save_data_init = false;
}

Handle task_get_mutex() {
    return mutex;
}

void task_refresh_files() {
    svcSignalEvent(events[EVENT_REFRESH_FILES]);
    files_init = true;
}

void task_refresh_titles() {
    svcSignalEvent(events[EVENT_REFRESH_TITLES]);
    titles_init = true;
}

void task_refresh_pending_titles() {
    svcSignalEvent(events[EVENT_REFRESH_PENDING_TITLES]);
    pending_titles_init = true;
}

void task_refresh_tickets() {
    svcSignalEvent(events[EVENT_REFRESH_TICKETS]);
    tickets_init = true;
}

void task_refresh_ext_save_data() {
    svcSignalEvent(events[EVENT_REFRESH_EXT_SAVE_DATA]);
    ext_save_data_init = true;
}

void task_refresh_system_save_data() {
    svcSignalEvent(events[EVENT_REFRESH_SYSTEM_SAVE_DATA]);
    system_save_data_init = true;
}

void task_request_cia_install(FS_MediaType dest, u64 size, void* data, Result (*read)(void* data, u32* bytesRead, void* buffer, u32 size)) {
    cia_cancelled = true;
    while(task_is_cia_installing()) {
        svcSleepThread(1000000);
    }

    cia_cancelled = false;
    cia_result = 0;
    cia_errno = 0;
    cia_dest = dest;
    cia_size = size;
    cia_data = data;
    cia_read = read;

    svcSignalEvent(events[EVENT_INSTALL_CIA]);

    while(!task_is_cia_installing()) {
        svcSleepThread(1000000);
    }
}

Result task_get_cia_install_result() {
    return cia_result;
}

int task_get_cia_install_errno() {
    return cia_errno;
}

bool task_is_cia_installing() {
    return cia_installing;
}

void task_cancel_cia_install() {
    cia_cancelled = true;
}

FS_Archive* task_get_files_archive() {
    return files_archive;
}

void task_set_files_archive(FS_Archive* archive) {
    files_archive = archive;
}

char* task_get_files_path() {
    return files_path;
}

void task_set_files_path(char* path) {
    files_path = path;
}

list_item* task_get_files() {
    if(!files_init) {
        task_refresh_files();
    }

    return files_list;
}

u32* task_get_files_count() {
    if(!files_init) {
        task_refresh_files();
    }

    return &files_count;
}

list_item* task_get_titles() {
    if(!titles_init) {
        task_refresh_titles();
    }

    return title_list;
}

u32* task_get_title_count() {
    if(!titles_init) {
        task_refresh_titles();
    }

    return &title_count;
}

list_item* task_get_pending_titles() {
    if(!pending_titles_init) {
        task_refresh_pending_titles();
    }

    return pending_title_list;
}

u32* task_get_pending_title_count() {
    if(!pending_titles_init) {
        task_refresh_pending_titles();
    }

    return &pending_title_count;
}

list_item* task_get_tickets() {
    if(!tickets_init) {
        task_refresh_tickets();
    }

    return ticket_list;
}

u32* task_get_ticket_count() {
    if(!tickets_init) {
        task_refresh_tickets();
    }

    return &ticket_count;
}

list_item* task_get_ext_save_data() {
    if(!ext_save_data_init) {
        task_refresh_ext_save_data();
    }

    return ext_save_data_list;
}

u32* task_get_ext_save_data_count() {
    if(!ext_save_data_init) {
        task_refresh_ext_save_data();
    }

    return &ext_save_data_count;
}

list_item* task_get_system_save_data() {
    if(!system_save_data_init) {
        task_refresh_system_save_data();
    }

    return system_save_data_list;
}

u32* task_get_system_save_data_count() {
    if(!system_save_data_init) {
        task_refresh_system_save_data();
    }

    return &system_save_data_count;
}

void ui_draw_ext_save_data_info(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    ext_save_data_info* info = (ext_save_data_info*) data;

    char buf[64];

    if(info->hasSmdh) {
        u32 smdhInfoBoxShadowWidth;
        u32 smdhInfoBoxShadowHeight;
        screen_get_texture_size(&smdhInfoBoxShadowWidth, &smdhInfoBoxShadowHeight, TEXTURE_SMDH_INFO_BOX_SHADOW);

        float smdhInfoBoxShadowX = x1 + (x2 - x1 - smdhInfoBoxShadowWidth) / 2;
        float smdhInfoBoxShadowY = y1 + (y2 - y1) / 4 - smdhInfoBoxShadowHeight / 2;
        screen_draw_texture(TEXTURE_SMDH_INFO_BOX_SHADOW, smdhInfoBoxShadowX, smdhInfoBoxShadowY, smdhInfoBoxShadowWidth, smdhInfoBoxShadowHeight);

        u32 smdhInfoBoxWidth;
        u32 smdhInfoBoxHeight;
        screen_get_texture_size(&smdhInfoBoxWidth, &smdhInfoBoxHeight, TEXTURE_SMDH_INFO_BOX);

        float smdhInfoBoxX = x1 + (x2 - x1 - smdhInfoBoxWidth) / 2;
        float smdhInfoBoxY = y1 + (y2 - y1) / 4 - smdhInfoBoxHeight / 2;
        screen_draw_texture(TEXTURE_SMDH_INFO_BOX, smdhInfoBoxX, smdhInfoBoxY, smdhInfoBoxWidth, smdhInfoBoxHeight);

        u32 smdhIconWidth;
        u32 smdhIconHeight;
        screen_get_texture_size(&smdhIconWidth, &smdhIconHeight, info->smdhInfo.texture);

        float smdhIconX = smdhInfoBoxX + 8;
        float smdhIconY = smdhInfoBoxY + 8;
        screen_draw_texture(info->smdhInfo.texture, smdhIconX, smdhIconY, smdhIconWidth, smdhIconHeight);

        float shortDescriptionHeight;
        screen_get_string_size(NULL, &shortDescriptionHeight, info->smdhInfo.shortDescription, 0.5f, 0.5f);

        float longDescriptionHeight;
        screen_get_string_size(NULL, &longDescriptionHeight, info->smdhInfo.longDescription, 0.5f, 0.5f);

        float publisherHeight;
        screen_get_string_size(NULL, &publisherHeight, info->smdhInfo.publisher, 0.5f, 0.5f);

        float smdhTextX = smdhIconX + smdhIconWidth + 8;

        float smdhShortDescriptionY = smdhIconY + (smdhIconHeight - shortDescriptionHeight - 2 - longDescriptionHeight - 2 - publisherHeight) / 2;
        screen_draw_string(info->smdhInfo.shortDescription, smdhTextX, smdhShortDescriptionY, 0.5f, 0.5f, 0xFF000000, false);

        float smdhLongDescriptionY = smdhShortDescriptionY + shortDescriptionHeight + 2;
        screen_draw_string(info->smdhInfo.longDescription, smdhTextX, smdhLongDescriptionY, 0.5f, 0.5f, 0xFF000000, false);

        float smdhPublisherY = smdhLongDescriptionY + longDescriptionHeight + 2;
        screen_draw_string(info->smdhInfo.publisher, smdhTextX, smdhPublisherY, 0.5f, 0.5f, 0xFF000000, false);
    }

    snprintf(buf, 64, "Ext Save Data ID: %016llX", info->extSaveDataId);

    float saveDataIdWidth;
    float saveDataIdHeight;
    screen_get_string_size(&saveDataIdWidth, &saveDataIdHeight, buf, 0.5f, 0.5f);

    float saveDataIdX = x1 + (x2 - x1 - saveDataIdWidth) / 2;
    float saveDataIdY = y1 + (y2 - y1) / 2 - 8;
    screen_draw_string(buf, saveDataIdX, saveDataIdY, 0.5f, 0.5f, 0xFF000000, false);

    snprintf(buf, 64, "Shared: %s", info->shared ? "Yes" : "No");

    float sharedWidth;
    float sharedHeight;
    screen_get_string_size(&sharedWidth, &sharedHeight, buf, 0.5f, 0.5f);

    float sharedX = x1 + (x2 - x1 - sharedWidth) / 2;
    float sharedY = saveDataIdY + saveDataIdHeight + 2;
    screen_draw_string(buf, sharedX, sharedY, 0.5f, 0.5f, 0xFF000000, false);
}

void ui_draw_file_info(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    file_info* info = (file_info*) data;

    char buf[64];

    if(strlen(info->name) > 48) {
        snprintf(buf, 64, "Name: %.45s...", info->name);
    } else {
        snprintf(buf, 64, "Name: %.48s", info->name);
    }

    float nameWidth;
    float nameHeight;
    screen_get_string_size(&nameWidth, &nameHeight, buf, 0.5f, 0.5f);

    float nameX = x1 + (x2 - x1 - nameWidth) / 2;
    float nameY = y1 + (y2 - y1) / 2 - 8;
    screen_draw_string(buf, nameX, nameY, 0.5f, 0.5f, 0xFF000000, false);

    if(!info->isDirectory) {
        snprintf(buf, 64, "Size: %.2f MB", info->size / 1024.0 / 1024.0);

        float sizeWidth;
        float sizeHeight;
        screen_get_string_size(&sizeWidth, &sizeHeight, buf, 0.5f, 0.5f);

        float sizeX = x1 + (x2 - x1 - sizeWidth) / 2;
        float sizeY = nameY + nameHeight + 2;
        screen_draw_string(buf, sizeX, sizeY, 0.5f, 0.5f, 0xFF000000, false);

        if(info->isCia) {
            if(info->ciaInfo.hasSmdh) {
                u32 smdhInfoBoxShadowWidth;
                u32 smdhInfoBoxShadowHeight;
                screen_get_texture_size(&smdhInfoBoxShadowWidth, &smdhInfoBoxShadowHeight, TEXTURE_SMDH_INFO_BOX_SHADOW);

                float smdhInfoBoxShadowX = x1 + (x2 - x1 - smdhInfoBoxShadowWidth) / 2;
                float smdhInfoBoxShadowY = y1 + (y2 - y1) / 4 - smdhInfoBoxShadowHeight / 2;
                screen_draw_texture(TEXTURE_SMDH_INFO_BOX_SHADOW, smdhInfoBoxShadowX, smdhInfoBoxShadowY, smdhInfoBoxShadowWidth, smdhInfoBoxShadowHeight);

                u32 smdhInfoBoxWidth;
                u32 smdhInfoBoxHeight;
                screen_get_texture_size(&smdhInfoBoxWidth, &smdhInfoBoxHeight, TEXTURE_SMDH_INFO_BOX);

                float smdhInfoBoxX = x1 + (x2 - x1 - smdhInfoBoxWidth) / 2;
                float smdhInfoBoxY = y1 + (y2 - y1) / 4 - smdhInfoBoxHeight / 2;
                screen_draw_texture(TEXTURE_SMDH_INFO_BOX, smdhInfoBoxX, smdhInfoBoxY, smdhInfoBoxWidth, smdhInfoBoxHeight);

                u32 smdhIconWidth;
                u32 smdhIconHeight;
                screen_get_texture_size(&smdhIconWidth, &smdhIconHeight, info->ciaInfo.smdhInfo.texture);

                float smdhIconX = smdhInfoBoxX + 8;
                float smdhIconY = smdhInfoBoxY + 8;
                screen_draw_texture(info->ciaInfo.smdhInfo.texture, smdhIconX, smdhIconY, smdhIconWidth, smdhIconHeight);

                float shortDescriptionHeight;
                screen_get_string_size(NULL, &shortDescriptionHeight, info->ciaInfo.smdhInfo.shortDescription, 0.5f, 0.5f);

                float longDescriptionHeight;
                screen_get_string_size(NULL, &longDescriptionHeight, info->ciaInfo.smdhInfo.longDescription, 0.5f, 0.5f);

                float publisherHeight;
                screen_get_string_size(NULL, &publisherHeight, info->ciaInfo.smdhInfo.publisher, 0.5f, 0.5f);

                float smdhTextX = smdhIconX + smdhIconWidth + 8;

                float smdhShortDescriptionY = smdhIconY + (smdhIconHeight - shortDescriptionHeight - 2 - longDescriptionHeight - 2 - publisherHeight) / 2;
                screen_draw_string(info->ciaInfo.smdhInfo.shortDescription, smdhTextX, smdhShortDescriptionY, 0.5f, 0.5f, 0xFF000000, false);

                float smdhLongDescriptionY = smdhShortDescriptionY + shortDescriptionHeight + 2;
                screen_draw_string(info->ciaInfo.smdhInfo.longDescription, smdhTextX, smdhLongDescriptionY, 0.5f, 0.5f, 0xFF000000, false);

                float smdhPublisherY = smdhLongDescriptionY + longDescriptionHeight + 2;
                screen_draw_string(info->ciaInfo.smdhInfo.publisher, smdhTextX, smdhPublisherY, 0.5f, 0.5f, 0xFF000000, false);
            }

            snprintf(buf, 64, "Title ID: %016llX", info->ciaInfo.titleId);

            float titleIdWidth;
            float titleIdHeight;
            screen_get_string_size(&titleIdWidth, &titleIdHeight, buf, 0.5f, 0.5f);

            float titleIdX = x1 + (x2 - x1 - titleIdWidth) / 2;
            float titleIdY = sizeY + sizeHeight + 2;
            screen_draw_string(buf, titleIdX, titleIdY, 0.5f, 0.5f, 0xFF000000, false);

            snprintf(buf, 64, "Version: %hu", info->ciaInfo.version);

            float versionWidth;
            float versionHeight;
            screen_get_string_size(&versionWidth, &versionHeight, buf, 0.5f, 0.5f);

            float versionX = x1 + (x2 - x1 - versionWidth) / 2;
            float versionY = titleIdY + titleIdHeight + 2;
            screen_draw_string(buf, versionX, versionY, 0.5f, 0.5f, 0xFF000000, false);

            snprintf(buf, 64, "Installed Size (SD): %.2f MB", info->ciaInfo.installedSizeSD / 1024.0 / 1024.0);

            float installedSizeSDWidth;
            float installedSizeSDHeight;
            screen_get_string_size(&installedSizeSDWidth, &installedSizeSDHeight, buf, 0.5f, 0.5f);

            float installedSizeSDX = x1 + (x2 - x1 - installedSizeSDWidth) / 2;
            float installedSizeSDY = versionY + versionHeight + 2;
            screen_draw_string(buf, installedSizeSDX, installedSizeSDY, 0.5f, 0.5f, 0xFF000000, false);

            snprintf(buf, 64, "Installed Size (NAND): %.2f MB", info->ciaInfo.installedSizeNAND / 1024.0 / 1024.0);

            float installedSizeNANDWidth;
            float installedSizeNANDHeight;
            screen_get_string_size(&installedSizeNANDWidth, &installedSizeNANDHeight, buf, 0.5f, 0.5f);

            float installedSizeNANDX = x1 + (x2 - x1 - installedSizeNANDWidth) / 2;
            float installedSizeNANDY = installedSizeSDY + installedSizeSDHeight + 2;
            screen_draw_string(buf, installedSizeNANDX, installedSizeNANDY, 0.5f, 0.5f, 0xFF000000, false);
        }
    } else {
        snprintf(buf, 64, "Directory");

        float directoryWidth;
        float directoryHeight;
        screen_get_string_size(&directoryWidth, &directoryHeight, buf, 0.5f, 0.5f);

        float directoryX = x1 + (x2 - x1 - directoryWidth) / 2;
        float directoryY = nameY + nameHeight + 2;
        screen_draw_string(buf, directoryX, directoryY, 0.5f, 0.5f, 0xFF000000, false);
    }
}

void ui_draw_system_save_data_info(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    system_save_data_info* info = (system_save_data_info*) data;

    char buf[64];

    snprintf(buf, 64, "System Save Data ID: %016llX", info->systemSaveDataId);

    float saveDataIdWidth;
    float saveDataIdHeight;
    screen_get_string_size(&saveDataIdWidth, &saveDataIdHeight, buf, 0.5f, 0.5f);

    float saveDataIdX = x1 + (x2 - x1 - saveDataIdWidth) / 2;
    float saveDataIdY = y1 + (y2 - y1) / 2 - 8;
    screen_draw_string(buf, saveDataIdX, saveDataIdY, 0.5f, 0.5f, 0xFF000000, false);
}

void ui_draw_title_info(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    title_info* info = (title_info*) data;

    char buf[64];

    if(info->hasSmdh) {
        u32 smdhInfoBoxShadowWidth;
        u32 smdhInfoBoxShadowHeight;
        screen_get_texture_size(&smdhInfoBoxShadowWidth, &smdhInfoBoxShadowHeight, TEXTURE_SMDH_INFO_BOX_SHADOW);

        float smdhInfoBoxShadowX = x1 + (x2 - x1 - smdhInfoBoxShadowWidth) / 2;
        float smdhInfoBoxShadowY = y1 + (y2 - y1) / 4 - smdhInfoBoxShadowHeight / 2;
        screen_draw_texture(TEXTURE_SMDH_INFO_BOX_SHADOW, smdhInfoBoxShadowX, smdhInfoBoxShadowY, smdhInfoBoxShadowWidth, smdhInfoBoxShadowHeight);

        u32 smdhInfoBoxWidth;
        u32 smdhInfoBoxHeight;
        screen_get_texture_size(&smdhInfoBoxWidth, &smdhInfoBoxHeight, TEXTURE_SMDH_INFO_BOX);

        float smdhInfoBoxX = x1 + (x2 - x1 - smdhInfoBoxWidth) / 2;
        float smdhInfoBoxY = y1 + (y2 - y1) / 4 - smdhInfoBoxHeight / 2;
        screen_draw_texture(TEXTURE_SMDH_INFO_BOX, smdhInfoBoxX, smdhInfoBoxY, smdhInfoBoxWidth, smdhInfoBoxHeight);

        u32 smdhIconWidth;
        u32 smdhIconHeight;
        screen_get_texture_size(&smdhIconWidth, &smdhIconHeight, info->smdhInfo.texture);

        float smdhIconX = smdhInfoBoxX + 8;
        float smdhIconY = smdhInfoBoxY + 8;
        screen_draw_texture(info->smdhInfo.texture, smdhIconX, smdhIconY, smdhIconWidth, smdhIconHeight);

        float shortDescriptionHeight;
        screen_get_string_size(NULL, &shortDescriptionHeight, info->smdhInfo.shortDescription, 0.5f, 0.5f);

        float longDescriptionHeight;
        screen_get_string_size(NULL, &longDescriptionHeight, info->smdhInfo.longDescription, 0.5f, 0.5f);

        float publisherHeight;
        screen_get_string_size(NULL, &publisherHeight, info->smdhInfo.publisher, 0.5f, 0.5f);

        float smdhTextX = smdhIconX + smdhIconWidth + 8;

        float smdhShortDescriptionY = smdhIconY + (smdhIconHeight - shortDescriptionHeight - 2 - longDescriptionHeight - 2 - publisherHeight) / 2;
        screen_draw_string(info->smdhInfo.shortDescription, smdhTextX, smdhShortDescriptionY, 0.5f, 0.5f, 0xFF000000, false);

        float smdhLongDescriptionY = smdhShortDescriptionY + shortDescriptionHeight + 2;
        screen_draw_string(info->smdhInfo.longDescription, smdhTextX, smdhLongDescriptionY, 0.5f, 0.5f, 0xFF000000, false);

        float smdhPublisherY = smdhLongDescriptionY + longDescriptionHeight + 2;
        screen_draw_string(info->smdhInfo.publisher, smdhTextX, smdhPublisherY, 0.5f, 0.5f, 0xFF000000, false);
    }

    snprintf(buf, 64, "Title ID: %016llX", info->titleId);

    float titleIdWidth;
    float titleIdHeight;
    screen_get_string_size(&titleIdWidth, &titleIdHeight, buf, 0.5f, 0.5f);

    float titleIdX = x1 + (x2 - x1 - titleIdWidth) / 2;
    float titleIdY = y1 + (y2 - y1) / 2 - 8;
    screen_draw_string(buf, titleIdX, titleIdY, 0.5f, 0.5f, 0xFF000000, false);

    snprintf(buf, 64, "Media Type: %s", info->mediaType == MEDIATYPE_NAND ? "NAND" : info->mediaType == MEDIATYPE_SD ? "SD" : "Game Card");

    float mediaTypeWidth;
    float mediaTypeHeight;
    screen_get_string_size(&mediaTypeWidth, &mediaTypeHeight, buf, 0.5f, 0.5f);

    float mediaTypeX = x1 + (x2 - x1 - mediaTypeWidth) / 2;
    float mediaTypeY = titleIdY + titleIdHeight + 2;
    screen_draw_string(buf, mediaTypeX, mediaTypeY, 0.5f, 0.5f, 0xFF000000, false);

    snprintf(buf, 64, "Product Code: %s", info->productCode);

    float productCodeWidth;
    float productCodeHeight;
    screen_get_string_size(&productCodeWidth, &productCodeHeight, buf, 0.5f, 0.5f);

    float productCodeX = x1 + (x2 - x1 - productCodeWidth) / 2;
    float productCodeY = mediaTypeY + mediaTypeHeight + 2;
    screen_draw_string(buf, productCodeX, productCodeY, 0.5f, 0.5f, 0xFF000000, false);

    snprintf(buf, 64, "Version: %hu", info->version);

    float versionWidth;
    float versionHeight;
    screen_get_string_size(&versionWidth, &versionHeight, buf, 0.5f, 0.5f);

    float versionX = x1 + (x2 - x1 - versionWidth) / 2;
    float versionY = productCodeY + productCodeHeight + 2;
    screen_draw_string(buf, versionX, versionY, 0.5f, 0.5f, 0xFF000000, false);

    snprintf(buf, 64, "Installed Size: %.2f MB", info->installedSize / 1024.0 / 1024.0);

    float installedSizeWidth;
    float installedSizeHeight;
    screen_get_string_size(&installedSizeWidth, &installedSizeHeight, buf, 0.5f, 0.5f);

    float installedSizeX = x1 + (x2 - x1 - installedSizeWidth) / 2;
    float installedSizeY = versionY + versionHeight + 2;
    screen_draw_string(buf, installedSizeX, installedSizeY, 0.5f, 0.5f, 0xFF000000, false);
}

void ui_draw_pending_title_info(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    pending_title_info* info = (pending_title_info*) data;

    char buf[64];

    snprintf(buf, 64, "Pending Title ID: %016llX", info->titleId);

    float titleIdWidth;
    float titleIdHeight;
    screen_get_string_size(&titleIdWidth, &titleIdHeight, buf, 0.5f, 0.5f);

    float titleIdX = x1 + (x2 - x1 - titleIdWidth) / 2;
    float titleIdY = y1 + (y2 - y1) / 2 - 8;
    screen_draw_string(buf, titleIdX, titleIdY, 0.5f, 0.5f, 0xFF000000, false);

    snprintf(buf, 64, "Media Type: %s", info->mediaType == MEDIATYPE_NAND ? "NAND" : info->mediaType == MEDIATYPE_SD ? "SD" : "Game Card");

    float mediaTypeWidth;
    float mediaTypeHeight;
    screen_get_string_size(&mediaTypeWidth, &mediaTypeHeight, buf, 0.5f, 0.5f);

    float mediaTypeX = x1 + (x2 - x1 - mediaTypeWidth) / 2;
    float mediaTypeY = titleIdY + titleIdHeight + 2;
    screen_draw_string(buf, mediaTypeX, mediaTypeY, 0.5f, 0.5f, 0xFF000000, false);

    snprintf(buf, 64, "Version: %hu", info->version);

    float versionWidth;
    float versionHeight;
    screen_get_string_size(&versionWidth, &versionHeight, buf, 0.5f, 0.5f);

    float versionX = x1 + (x2 - x1 - versionWidth) / 2;
    float versionY = mediaTypeY + mediaTypeHeight + 2;
    screen_draw_string(buf, versionX, versionY, 0.5f, 0.5f, 0xFF000000, false);
}

void ui_draw_ticket_info(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    ticket_info* info = (ticket_info*) data;

    char buf[64];

    snprintf(buf, 64, "Ticket ID: %016llX", info->ticketId);

    float titleIdWidth;
    float titleIdHeight;
    screen_get_string_size(&titleIdWidth, &titleIdHeight, buf, 0.5f, 0.5f);

    float titleIdX = x1 + (x2 - x1 - titleIdWidth) / 2;
    float titleIdY = y1 + (y2 - y1) / 2 - 8;
    screen_draw_string(buf, titleIdX, titleIdY, 0.5f, 0.5f, 0xFF000000, false);
}
