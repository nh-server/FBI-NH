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

    file_info* dir;
} populate_files_data;

static void task_populate_files_thread(void* arg) {
    populate_files_data* data = (populate_files_data*) arg;

    data->dir->containsCias = false;

    Result res = 0;

    FS_Path* fsPath = util_make_path_utf8(data->dir->path);
    if(fsPath != NULL) {
        Handle dirHandle = 0;
        if(R_SUCCEEDED(res = FSUSER_OpenDirectory(&dirHandle, *data->dir->archive, *fsPath))) {
            u32 entryCount = 0;
            FS_DirectoryEntry* entries = (FS_DirectoryEntry*) calloc(data->max, sizeof(FS_DirectoryEntry));
            if(entries != NULL) {
                if(R_SUCCEEDED(res = FSDIR_Read(dirHandle, &entryCount, data->max, entries)) && entryCount > 0) {
                    qsort(entries, entryCount, sizeof(FS_DirectoryEntry), util_compare_directory_entries);

                    SMDH smdh;
                    for(u32 i = 0; i < entryCount && i < data->max && R_SUCCEEDED(res); i++) {
                        if(task_is_quit_all() || svcWaitSynchronization(data->cancelEvent, 0) == 0) {
                            break;
                        }

                        if(entries[i].attributes & FS_ATTRIBUTE_HIDDEN) {
                            continue;
                        }

                        file_info* fileInfo = (file_info*) calloc(1, sizeof(file_info));
                        if(fileInfo != NULL) {
                            char entryName[0x213] = {'\0'};
                            utf16_to_utf8((uint8_t*) entryName, entries[i].name, sizeof(entryName) - 1);

                            fileInfo->archive = data->dir->archive;
                            strncpy(fileInfo->name, entryName, NAME_MAX);

                            list_item* item = &data->items[*data->count];

                            if(entries[i].attributes & FS_ATTRIBUTE_DIRECTORY) {
                                item->rgba = COLOR_DIRECTORY;

                                snprintf(fileInfo->path, PATH_MAX, "%s%s/", data->dir->path, entryName);
                                fileInfo->isDirectory = true;
                                fileInfo->containsCias = false;
                                fileInfo->size = 0;
                                fileInfo->isCia = false;
                            } else {
                                item->rgba = COLOR_TEXT;

                                snprintf(fileInfo->path, PATH_MAX, "%s%s", data->dir->path, entryName);
                                fileInfo->isDirectory = false;
                                fileInfo->containsCias = false;
                                fileInfo->size = 0;
                                fileInfo->isCia = false;

                                FS_Path* fileFsPath = util_make_path_utf8(fileInfo->path);
                                if(fileFsPath != NULL) {
                                    Handle fileHandle;
                                    if(R_SUCCEEDED(FSUSER_OpenFile(&fileHandle, *data->dir->archive, *fileFsPath, FS_OPEN_READ, 0))) {
                                        FSFILE_GetSize(fileHandle, &fileInfo->size);

                                        size_t len = strlen(fileInfo->path);
                                        if(len > 4) {
                                            if(strcasecmp(&fileInfo->path[len - 4], ".cia") == 0) {
                                                AM_TitleEntry titleEntry;
                                                if(R_SUCCEEDED(AM_GetCiaFileInfo(MEDIATYPE_SD, &titleEntry, fileHandle))) {
                                                    data->dir->containsCias = true;

                                                    fileInfo->isCia = true;
                                                    fileInfo->ciaInfo.titleId = titleEntry.titleID;
                                                    fileInfo->ciaInfo.version = titleEntry.version;
                                                    fileInfo->ciaInfo.installedSize = titleEntry.size;
                                                    fileInfo->ciaInfo.hasMeta = false;

                                                    if(((titleEntry.titleID >> 32) & 0x8010) != 0 && R_SUCCEEDED(AM_GetCiaFileInfo(MEDIATYPE_NAND, &titleEntry, fileHandle))) {
                                                        fileInfo->ciaInfo.installedSize = titleEntry.size;
                                                    }

                                                    if(R_SUCCEEDED(AM_GetCiaIcon(&smdh, fileHandle))) {
                                                        u8 systemLanguage = CFG_LANGUAGE_EN;
                                                        CFGU_GetSystemLanguage(&systemLanguage);

                                                        fileInfo->ciaInfo.hasMeta = true;
                                                        utf16_to_utf8((uint8_t*) fileInfo->ciaInfo.meta.shortDescription, smdh.titles[systemLanguage].shortDescription, sizeof(fileInfo->ciaInfo.meta.shortDescription));
                                                        utf16_to_utf8((uint8_t*) fileInfo->ciaInfo.meta.longDescription, smdh.titles[systemLanguage].longDescription, sizeof(fileInfo->ciaInfo.meta.longDescription));
                                                        utf16_to_utf8((uint8_t*) fileInfo->ciaInfo.meta.publisher, smdh.titles[systemLanguage].publisher, sizeof(fileInfo->ciaInfo.meta.publisher));
                                                        fileInfo->ciaInfo.meta.texture = screen_load_texture_tiled_auto(smdh.largeIcon, sizeof(smdh.largeIcon), 48, 48, GPU_RGB565, false);
                                                    }
                                                }
                                            } else if(strcasecmp(&fileInfo->path[len - 4], ".tik") == 0) {
                                                u32 bytesRead = 0;

                                                u8 sigType = 0;
                                                if(R_SUCCEEDED(FSFILE_Read(fileHandle, &bytesRead, 3, &sigType, sizeof(sigType))) && bytesRead == sizeof(sigType) && sigType <= 5) {
                                                    static u32 dataOffsets[6] = {0x240, 0x140, 0x80, 0x240, 0x140, 0x80};
                                                    static u32 titleIdOffset = 0x9C;

                                                    u64 titleId = 0;
                                                    if(R_SUCCEEDED(FSFILE_Read(fileHandle, &bytesRead, dataOffsets[sigType] + titleIdOffset, &titleId, sizeof(titleId))) && bytesRead == sizeof(titleId)) {
                                                        data->dir->containsTickets = true;

                                                        fileInfo->isTicket = true;
                                                        fileInfo->ticketInfo.ticketId = __builtin_bswap64(titleId);
                                                    }
                                                }
                                            }
                                        }

                                        FSFILE_Close(fileHandle);
                                    }

                                    util_free_path_utf8(fileFsPath);
                                }
                            }

                            strncpy(item->name, entryName, NAME_MAX);
                            item->data = fileInfo;

                            (*data->count)++;
                        } else {
                            res = R_FBI_OUT_OF_MEMORY;
                        }
                    }
                }

                free(entries);
            } else {
                res = R_FBI_OUT_OF_MEMORY;
            }

            FSDIR_Close(dirHandle);
        }

        util_free_path_utf8(fsPath);
    } else {
        res = R_FBI_OUT_OF_MEMORY;
    }

    if(R_FAILED(res)) {
        error_display_res(NULL, NULL, NULL, res, "Failed to load file listing.");
    }

    svcCloseHandle(data->cancelEvent);
    free(data);
}

void task_clear_files(list_item* items, u32* count) {
    if(items == NULL || count == NULL) {
        return;
    }

    u32 prevCount = *count;
    *count = 0;

    for(u32 i = 0; i < prevCount; i++) {
        if(items[i].data != NULL) {
            file_info* fileInfo = (file_info*) items[i].data;
            if(fileInfo->isCia && fileInfo->ciaInfo.hasMeta) {
                screen_unload_texture(fileInfo->ciaInfo.meta.texture);
            }

            free(items[i].data);
            items[i].data = NULL;
        }

        memset(items[i].name, '\0', NAME_MAX);
        items[i].rgba = 0;
    }
}

Handle task_populate_files(list_item* items, u32* count, u32 max, file_info* dir) {
    if(items == NULL || count == NULL || max == 0 || dir == NULL) {
        return 0;
    }

    task_clear_files(items, count);

    populate_files_data* data = (populate_files_data*) calloc(1, sizeof(populate_files_data));
    data->items = items;
    data->count = count;
    data->max = max;
    data->dir = dir;

    Result eventRes = svcCreateEvent(&data->cancelEvent, 1);
    if(R_FAILED(eventRes)) {
        error_display_res(NULL, NULL, NULL, eventRes, "Failed to create file list cancel event.");

        free(data);
        return 0;
    }

    if(threadCreate(task_populate_files_thread, data, 0x4000, 0x18, 1, true) == NULL) {
        error_display(NULL, NULL, NULL, "Failed to create file list thread.");

        svcCloseHandle(data->cancelEvent);
        free(data);
        return 0;
    }

    return data->cancelEvent;
}