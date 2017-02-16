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

#define MAX_FILES 1024

Result task_create_file_item(list_item** out, FS_Archive archive, const char* path, u32 attributes) {
    Result res = 0;

    list_item* item = (list_item*) calloc(1, sizeof(list_item));
    if(item != NULL) {
        file_info* fileInfo = (file_info*) calloc(1, sizeof(file_info));
        if(fileInfo != NULL) {
            fileInfo->archive = archive;
            util_get_path_file(fileInfo->name, path, FILE_NAME_MAX);
            fileInfo->attributes = attributes != UINT32_MAX ? attributes : 0;

            fileInfo->size = 0;
            fileInfo->isCia = false;
            fileInfo->isTicket = false;

            if((attributes != UINT32_MAX && (attributes & FS_ATTRIBUTE_DIRECTORY)) || util_is_dir(archive, path)) {
                item->color = COLOR_DIRECTORY;

                size_t len = strlen(path);
                if(len == 0 || path[len - 1] != '/') {
                    snprintf(fileInfo->path, FILE_PATH_MAX, "%s/", path);
                } else {
                    strncpy(fileInfo->path, path, FILE_PATH_MAX);
                }

                if(attributes == UINT32_MAX) {
                    fileInfo->attributes = FS_ATTRIBUTE_DIRECTORY;
                }
            } else {
                item->color = COLOR_FILE;

                strncpy(fileInfo->path, path, FILE_PATH_MAX);

                FS_Path* fileFsPath = util_make_path_utf8(fileInfo->path);
                if(fileFsPath != NULL) {
                    Handle fileHandle;
                    if(R_SUCCEEDED(FSUSER_OpenFile(&fileHandle, archive, *fileFsPath, FS_OPEN_READ, 0))) {
                        if(attributes == UINT32_MAX && R_FAILED(FSFILE_GetAttributes(fileHandle, &fileInfo->attributes))) {
                            fileInfo->attributes = 0;
                        }

                        FSFILE_GetSize(fileHandle, &fileInfo->size);

                        if(util_filter_cias(NULL, fileInfo->path, fileInfo->attributes)) {
                            AM_TitleEntry titleEntry;
                            if(R_SUCCEEDED(AM_GetCiaFileInfo(MEDIATYPE_SD, &titleEntry, fileHandle))) {
                                fileInfo->isCia = true;
                                fileInfo->ciaInfo.titleId = titleEntry.titleID;
                                fileInfo->ciaInfo.version = titleEntry.version;
                                fileInfo->ciaInfo.installedSize = titleEntry.size;
                                fileInfo->ciaInfo.hasMeta = false;

                                if(util_get_title_destination(titleEntry.titleID) != MEDIATYPE_SD && R_SUCCEEDED(AM_GetCiaFileInfo(MEDIATYPE_NAND, &titleEntry, fileHandle))) {
                                    fileInfo->ciaInfo.installedSize = titleEntry.size;
                                }

                                SMDH* smdh = (SMDH*) calloc(1, sizeof(SMDH));
                                if(smdh != NULL) {
                                    if(R_SUCCEEDED(util_get_cia_file_smdh(smdh, fileHandle))) {
                                        if(smdh->magic[0] == 'S' && smdh->magic[1] == 'M' && smdh->magic[2] == 'D' && smdh->magic[3] == 'H') {
                                            SMDH_title* smdhTitle = util_select_smdh_title(smdh);

                                            fileInfo->ciaInfo.hasMeta = true;
                                            utf16_to_utf8((uint8_t*) fileInfo->ciaInfo.meta.shortDescription, smdhTitle->shortDescription, sizeof(fileInfo->ciaInfo.meta.shortDescription) - 1);
                                            utf16_to_utf8((uint8_t*) fileInfo->ciaInfo.meta.longDescription, smdhTitle->longDescription, sizeof(fileInfo->ciaInfo.meta.longDescription) - 1);
                                            utf16_to_utf8((uint8_t*) fileInfo->ciaInfo.meta.publisher, smdhTitle->publisher, sizeof(fileInfo->ciaInfo.meta.publisher) - 1);
                                            fileInfo->ciaInfo.meta.region = smdh->region;
                                            fileInfo->ciaInfo.meta.texture = screen_allocate_free_texture();
                                            screen_load_texture_tiled(fileInfo->ciaInfo.meta.texture, smdh->largeIcon, sizeof(smdh->largeIcon), 48, 48, GPU_RGB565, false);
                                        }
                                    }

                                    free(smdh);
                                }
                            }
                        } else if(util_filter_tickets(NULL, fileInfo->path, fileInfo->attributes)) {
                            u32 bytesRead = 0;

                            u8 sigType = 0;
                            if(R_SUCCEEDED(FSFILE_Read(fileHandle, &bytesRead, 3, &sigType, sizeof(sigType))) && bytesRead == sizeof(sigType) && sigType <= 5) {
                                static u32 dataOffsets[6] = {0x240, 0x140, 0x80, 0x240, 0x140, 0x80};
                                static u32 titleIdOffset = 0x9C;

                                u64 titleId = 0;
                                if(R_SUCCEEDED(FSFILE_Read(fileHandle, &bytesRead, dataOffsets[sigType] + titleIdOffset, &titleId, sizeof(titleId))) && bytesRead == sizeof(titleId)) {
                                    fileInfo->isTicket = true;
                                    fileInfo->ticketInfo.titleId = __builtin_bswap64(titleId);
                                    fileInfo->ticketInfo.inUse = false;
                                }
                            }
                        }

                        FSFILE_Close(fileHandle);
                    }

                    util_free_path_utf8(fileFsPath);
                }
            }

            strncpy(item->name, fileInfo->name, LIST_ITEM_NAME_MAX);
            item->data = fileInfo;

            *out = item;
        } else {
            free(item);

            res = R_FBI_OUT_OF_MEMORY;
        }
    } else {
        res = R_FBI_OUT_OF_MEMORY;
    }

    return res;
}

static int task_populate_files_compare_directory_entries(const void* e1, const void* e2) {
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

static void task_populate_files_thread(void* arg) {
    populate_files_data* data = (populate_files_data*) arg;

    Result res = 0;

    list_item* baseItem = NULL;
    if(R_SUCCEEDED(res = task_create_file_item(&baseItem, data->archive, data->path, UINT32_MAX))) {
        file_info* baseInfo = (file_info*) baseItem->data;
        if(baseInfo->attributes & FS_ATTRIBUTE_DIRECTORY) {
            strncpy(baseItem->name, "<current directory>", LIST_ITEM_NAME_MAX);
        } else {
            strncpy(baseItem->name, "<current file>", LIST_ITEM_NAME_MAX);
        }

        linked_list queue;
        linked_list_init(&queue);

        linked_list_add(&queue, baseItem);

        bool quit = false;
        while(!quit && R_SUCCEEDED(res) && linked_list_size(&queue) > 0) {
            u32 tail = linked_list_size(&queue) - 1;
            list_item* currItem = (list_item*) linked_list_get(&queue, tail);
            file_info* curr = (file_info*) currItem->data;
            linked_list_remove_at(&queue, tail);

            if(data->includeBase || currItem != baseItem) {
                linked_list_add(data->items, currItem);
            }

            if(curr->attributes & FS_ATTRIBUTE_DIRECTORY) {
                FS_Path* fsPath = util_make_path_utf8(curr->path);
                if(fsPath != NULL) {
                    Handle dirHandle = 0;
                    if(R_SUCCEEDED(res = FSUSER_OpenDirectory(&dirHandle, curr->archive, *fsPath))) {
                        u32 entryCount = 0;
                        FS_DirectoryEntry* entries = (FS_DirectoryEntry*) calloc(MAX_FILES, sizeof(FS_DirectoryEntry));
                        if(entries != NULL) {
                            if(R_SUCCEEDED(res = FSDIR_Read(dirHandle, &entryCount, MAX_FILES, entries)) && entryCount > 0) {
                                qsort(entries, entryCount, sizeof(FS_DirectoryEntry), task_populate_files_compare_directory_entries);

                                for(u32 i = 0; i < entryCount && R_SUCCEEDED(res); i++) {
                                    svcWaitSynchronization(task_get_pause_event(), U64_MAX);
                                    if(task_is_quit_all() || svcWaitSynchronization(data->cancelEvent, 0) == 0) {
                                        quit = true;
                                        break;
                                    }

                                    char name[FILE_NAME_MAX] = {'\0'};
                                    utf16_to_utf8((uint8_t*) name, entries[i].name, FILE_NAME_MAX - 1);

                                    if(data->filter == NULL || data->filter(data->filterData, name, entries[i].attributes)) {
                                        char path[FILE_PATH_MAX] = {'\0'};
                                        snprintf(path, FILE_PATH_MAX, "%s%s", curr->path, name);

                                        list_item* item = NULL;
                                        if(R_SUCCEEDED(res = task_create_file_item(&item, curr->archive, path, entries[i].attributes))) {
                                            if(data->recursive && (((file_info*) item->data)->attributes & FS_ATTRIBUTE_DIRECTORY)) {
                                                linked_list_add(&queue, item);
                                            } else {
                                                linked_list_add(data->items, item);
                                            }
                                        }
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
            }
        }

        linked_list_destroy(&queue);

        if(!data->includeBase) {
            task_free_file(baseItem);
        }
    }

    svcCloseHandle(data->cancelEvent);

    data->result = res;
    data->finished = true;
}

void task_free_file(list_item* item) {
    if(item == NULL) {
        return;
    }

    if(item->data != NULL) {
        file_info* fileInfo = (file_info*) item->data;
        if(fileInfo->isCia && fileInfo->ciaInfo.hasMeta) {
            screen_unload_texture(fileInfo->ciaInfo.meta.texture);
        }

        free(item->data);
    }

    free(item);
}

void task_clear_files(linked_list* items) {
    if(items == NULL) {
        return;
    }

    linked_list_iter iter;
    linked_list_iterate(items, &iter);

    while(linked_list_iter_has_next(&iter)) {
        list_item* item = (list_item*) linked_list_iter_next(&iter);

        linked_list_iter_remove(&iter);
        task_free_file(item);
    }
}

Result task_populate_files(populate_files_data* data) {
    if(data == NULL || data->items == NULL) {
        return R_FBI_INVALID_ARGUMENT;
    }

    task_clear_files(data->items);

    data->finished = false;
    data->result = 0;
    data->cancelEvent = 0;

    Result res = 0;
    if(R_SUCCEEDED(res = svcCreateEvent(&data->cancelEvent, RESET_STICKY))) {
        if(threadCreate(task_populate_files_thread, data, 0x10000, 0x19, 1, true) == NULL) {
            res = R_FBI_THREAD_CREATE_FAILED;
        }
    }

    if(R_FAILED(res)) {
        data->finished = true;

        if(data->cancelEvent != 0) {
            svcCloseHandle(data->cancelEvent);
            data->cancelEvent = 0;
        }
    }

    return res;
}