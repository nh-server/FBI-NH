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

#define MAX_FILES 1024

typedef struct {
    linked_list* items;
    file_info* dir;

    Handle cancelEvent;
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
            FS_DirectoryEntry* entries = (FS_DirectoryEntry*) calloc(MAX_FILES, sizeof(FS_DirectoryEntry));
            if(entries != NULL) {
                if(R_SUCCEEDED(res = FSDIR_Read(dirHandle, &entryCount, MAX_FILES, entries)) && entryCount > 0) {
                    qsort(entries, entryCount, sizeof(FS_DirectoryEntry), util_compare_directory_entries);

                    SMDH smdh;
                    for(u32 i = 0; i < entryCount && R_SUCCEEDED(res); i++) {
                        if(task_is_quit_all() || svcWaitSynchronization(data->cancelEvent, 0) == 0) {
                            break;
                        }

                        if(entries[i].attributes & FS_ATTRIBUTE_HIDDEN) {
                            continue;
                        }

                        list_item* item = (list_item*) calloc(1, sizeof(list_item));
                        if(item != NULL) {
                            file_info* fileInfo = (file_info*) calloc(1, sizeof(file_info));
                            if(fileInfo != NULL) {
                                fileInfo->archive = data->dir->archive;
                                utf16_to_utf8((uint8_t*) fileInfo->name, entries[i].name, NAME_MAX - 1);
                                fileInfo->containsCias = false;
                                fileInfo->size = 0;
                                fileInfo->isCia = false;

                                if(entries[i].attributes & FS_ATTRIBUTE_DIRECTORY) {
                                    item->color = COLOR_DIRECTORY;

                                    snprintf(fileInfo->path, PATH_MAX, "%s%s/", data->dir->path, fileInfo->name);
                                    fileInfo->isDirectory = true;
                                } else {
                                    item->color = COLOR_TEXT;

                                    snprintf(fileInfo->path, PATH_MAX, "%s%s", data->dir->path, fileInfo->name);
                                    fileInfo->isDirectory = false;

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
                                                            utf16_to_utf8((uint8_t*) fileInfo->ciaInfo.meta.shortDescription, smdh.titles[systemLanguage].shortDescription, sizeof(fileInfo->ciaInfo.meta.shortDescription) - 1);
                                                            utf16_to_utf8((uint8_t*) fileInfo->ciaInfo.meta.longDescription, smdh.titles[systemLanguage].longDescription, sizeof(fileInfo->ciaInfo.meta.longDescription) - 1);
                                                            utf16_to_utf8((uint8_t*) fileInfo->ciaInfo.meta.publisher, smdh.titles[systemLanguage].publisher, sizeof(fileInfo->ciaInfo.meta.publisher) - 1);
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
                                                            fileInfo->ticketInfo.titleId = __builtin_bswap64(titleId);
                                                        }
                                                    }
                                                }
                                            }

                                            FSFILE_Close(fileHandle);
                                        }

                                        util_free_path_utf8(fileFsPath);
                                    }
                                }

                                strncpy(item->name, fileInfo->name, NAME_MAX);
                                item->data = fileInfo;

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

void task_clear_files(linked_list* items) {
    if(items == NULL) {
        return;
    }

    linked_list_iter iter;
    linked_list_iterate(items, &iter);

    while(linked_list_iter_has_next(&iter)) {
        list_item* item = (list_item*) linked_list_iter_next(&iter);

        if(item->data != NULL) {
            file_info* fileInfo = (file_info*) item->data;
            if(fileInfo->isCia && fileInfo->ciaInfo.hasMeta) {
                screen_unload_texture(fileInfo->ciaInfo.meta.texture);
            }

            free(item->data);
        }

        free(item);

        linked_list_iter_remove(&iter);
    }
}

Handle task_populate_files(linked_list* items, file_info* dir) {
    if(items == NULL || dir == NULL) {
        return 0;
    }

    task_clear_files(items);

    populate_files_data* data = (populate_files_data*) calloc(1, sizeof(populate_files_data));
    if(data == NULL) {
        error_display(NULL, NULL, NULL, "Failed to allocate file list data.");

        return 0;
    }

    data->items = items;
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