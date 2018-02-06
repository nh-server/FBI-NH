#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <3ds.h>
#include <jansson.h>

#include "uitask.h"
#include "../../list.h"
#include "../../resources.h"
#include "../../../core/error.h"
#include "../../../core/fs.h"
#include "../../../core/linkedlist.h"
#include "../../../core/screen.h"
#include "../../../core/stringutil.h"
#include "../../../core/task/task.h"
#include "../../../libs/stb_image/stb_image.h"

#define json_object_get_string(obj, name, def) (json_is_string(json_object_get(obj, name)) ? json_string_value(json_object_get(obj, name)) : def)
#define json_object_get_integer(obj, name, def) (json_is_integer(json_object_get(obj, name)) ? json_integer_value(json_object_get(obj, name)) : def)

void task_populate_titledb_update_status(list_item* item) {
    titledb_info* info = (titledb_info*) item->data;

    if(info->cia.exists) {
        AM_TitleEntry entry;
        info->cia.installed = R_SUCCEEDED(AM_GetTitleInfo(fs_get_title_destination(info->cia.titleId), 1, &info->cia.titleId, &entry));
        info->cia.installedVersion = info->cia.installed ? entry.version : (u16) 0;
    }

    if(info->tdsx.exists) {
        info->tdsx.installed = false;

        char name[FILE_NAME_MAX];
        string_escape_file_name(name, info->meta.shortDescription, sizeof(name));

        char path3dsx[FILE_PATH_MAX];
        snprintf(path3dsx, sizeof(path3dsx), "/3ds/%s/%s.3dsx", name, name);

        FS_Path* fsPath = fs_make_path_utf8(path3dsx);
        if(fsPath != NULL) {
            Handle handle = 0;
            if(R_SUCCEEDED(FSUSER_OpenFileDirectly(&handle, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), *fsPath, FS_OPEN_READ, 0))) {
                FSFILE_Close(handle);

                info->tdsx.installed = true;
            }

            fs_free_path_utf8(fsPath);
        }
    }

    // TODO: Outdated color(?)
    if((info->cia.exists && info->cia.installed) || (info->tdsx.exists && info->tdsx.installed)) {
        item->color = COLOR_TITLEDB_INSTALLED;
    } else {
        item->color = COLOR_TITLEDB_NOT_INSTALLED;
    }
}

static int task_populate_titledb_compare(void* userData, const void* p1, const void* p2) {
    list_item* info1 = (list_item*) p1;
    list_item* info2 = (list_item*) p2;

    return strncasecmp(info1->name, info2->name, LIST_ITEM_NAME_MAX);
}

static void task_populate_titledb_thread(void* arg) {
    populate_titledb_data* data = (populate_titledb_data*) arg;

    Result res = 0;

    json_t* root = NULL;
    if(R_SUCCEEDED(res = task_download_json_sync("https://api.titledb.com/v1/entry?nested=true"
                                                         "&only=id&only=name&only=author&only=headline&only=category"
                                                         "&only=cia.id&only=cia.updated_at&only=cia.version&only=cia.size&only=cia.titleid"
                                                         "&only=tdsx.id&only=tdsx.updated_at&only=tdsx.version&only=tdsx.size&only=tdsx.smdh.id",
                                            &root, 1024 * 1024))) {
        if(json_is_array(root)) {
            linked_list titles;
            linked_list_init(&titles);

            for(u32 i = 0; i < json_array_size(root) && R_SUCCEEDED(res); i++) {
                svcWaitSynchronization(task_get_pause_event(), U64_MAX);
                if(task_is_quit_all() || svcWaitSynchronization(data->cancelEvent, 0) == 0) {
                    break;
                }

                json_t* entry = json_array_get(root, i);
                if(json_is_object(entry)) {
                    list_item* item = (list_item*) calloc(1, sizeof(list_item));
                    if(item != NULL) {
                        titledb_info* titledbInfo = (titledb_info*) calloc(1, sizeof(titledb_info));
                        if(titledbInfo != NULL) {
                            titledbInfo->id = (u32) json_object_get_integer(entry, "id", 0);
                            strncpy(titledbInfo->category, json_object_get_string(entry, "category", "Unknown"), sizeof(titledbInfo->category));
                            strncpy(titledbInfo->headline, json_object_get_string(entry, "headline", ""), sizeof(titledbInfo->headline));
                            strncpy(titledbInfo->meta.shortDescription, json_object_get_string(entry, "name", ""), sizeof(titledbInfo->meta.shortDescription));
                            strncpy(titledbInfo->meta.publisher, json_object_get_string(entry, "author", ""), sizeof(titledbInfo->meta.publisher));

                            json_t* cias = json_object_get(entry, "cia");
                            if(json_is_array(cias)) {
                                for(u32 j = 0; j < json_array_size(cias); j++) {
                                    json_t* cia = json_array_get(cias, j);
                                    if(json_is_object(cia)) {
                                        const char* updatedAt = json_object_get_string(cia, "updated_at", "");
                                        if(!titledbInfo->cia.exists || strncmp(updatedAt, titledbInfo->cia.updatedAt, sizeof(titledbInfo->cia.updatedAt)) >= 0) {
                                            titledbInfo->cia.exists = true;

                                            titledbInfo->cia.id = (u32) json_object_get_integer(cia, "id", 0);
                                            strncpy(titledbInfo->cia.updatedAt, updatedAt, sizeof(titledbInfo->cia.updatedAt));
                                            strncpy(titledbInfo->cia.version, json_object_get_string(cia, "version", "Unknown"), sizeof(titledbInfo->cia.version));
                                            titledbInfo->cia.size = (u32) json_object_get_integer(cia, "size", 0);
                                            titledbInfo->cia.titleId = strtoull(json_object_get_string(cia, "titleid", "0"), NULL, 16);
                                        }
                                    }
                                }
                            }

                            json_t* tdsxs = json_object_get(entry, "tdsx");
                            if(json_is_array(tdsxs)) {
                                for(u32 j = 0; j < json_array_size(tdsxs); j++) {
                                    json_t* tdsx = json_array_get(tdsxs, j);
                                    if(json_is_object(tdsx)) {
                                        const char* updatedAt = json_object_get_string(tdsx, "updated_at", "");
                                        if(!titledbInfo->tdsx.exists || strncmp(updatedAt, titledbInfo->tdsx.updatedAt, sizeof(titledbInfo->tdsx.updatedAt)) >= 0) {
                                            titledbInfo->tdsx.exists = true;

                                            titledbInfo->tdsx.id = (u32) json_object_get_integer(tdsx, "id", 0);
                                            strncpy(titledbInfo->tdsx.updatedAt, updatedAt, sizeof(titledbInfo->tdsx.updatedAt));
                                            strncpy(titledbInfo->tdsx.version, json_object_get_string(tdsx, "version", "Unknown"), sizeof(titledbInfo->tdsx.version));
                                            titledbInfo->tdsx.size = (u32) json_object_get_integer(tdsx, "size", 0);

                                            json_t* smdh = json_object_get(tdsx, "smdh");
                                            if(json_is_object(smdh)) {
                                                titledbInfo->tdsx.smdh.exists = true;

                                                titledbInfo->tdsx.smdh.id = (u32) json_object_get_integer(smdh, "id", 0);
                                            }
                                        }
                                    }
                                }
                            }

                            strncpy(item->name, titledbInfo->meta.shortDescription, LIST_ITEM_NAME_MAX);
                            item->data = titledbInfo;

                            task_populate_titledb_update_status(item);

                            linked_list_add_sorted(&titles, item, NULL, task_populate_titledb_compare);
                        } else {
                            free(item);

                            res = R_APP_OUT_OF_MEMORY;
                        }
                    } else {
                        res = R_APP_OUT_OF_MEMORY;
                    }
                }
            }

            linked_list_iter iter;
            linked_list_iterate(&titles, &iter);

            while(linked_list_iter_has_next(&iter)) {
                list_item* item = linked_list_iter_next(&iter);

                if(R_SUCCEEDED(res)) {
                    linked_list_add(data->items, item);
                } else {
                    task_free_titledb(item);
                }
            }

            linked_list_destroy(&titles);
        } else {
            res = R_APP_BAD_DATA;
        }

        json_decref(root);
    }

    data->itemsListed = true;

    if(R_SUCCEEDED(res)) {
        linked_list_iter iter;
        linked_list_iterate(data->items, &iter);

        while(linked_list_iter_has_next(&iter)) {
            svcWaitSynchronization(task_get_pause_event(), U64_MAX);

            Handle events[2] = {data->resumeEvent, data->cancelEvent};
            s32 index = 0;
            svcWaitSynchronizationN(&index, events, 2, false, U64_MAX);

            if(task_is_quit_all() || svcWaitSynchronization(data->cancelEvent, 0) == 0) {
                break;
            }

            list_item* item = (list_item*) linked_list_iter_next(&iter);
            titledb_info* titledbInfo = (titledb_info*) item->data;

            char url[128];
            if(titledbInfo->cia.exists) {
                snprintf(url, sizeof(url), "https://3ds.titledb.com/v1/cia/%lu/icon_l.bin", titledbInfo->cia.id);
            } else if(titledbInfo->tdsx.exists && titledbInfo->tdsx.smdh.exists) {
                snprintf(url, sizeof(url), "https://3ds.titledb.com/v1/smdh/%lu/icon_l.bin", titledbInfo->tdsx.smdh.id);
            } else {
                continue;
            }

            u8 icon[0x1200];
            u32 iconSize = 0;
            if(R_SUCCEEDED(task_download_sync(url, &iconSize, &icon, sizeof(icon))) && iconSize == sizeof(icon)) {
                titledbInfo->meta.texture = screen_allocate_free_texture();
                screen_load_texture_tiled(titledbInfo->meta.texture, icon, sizeof(icon), 48, 48, GPU_RGB565, false);
            }
        }
    }

    svcCloseHandle(data->resumeEvent);
    svcCloseHandle(data->cancelEvent);

    data->result = res;
    data->finished = true;
}

void task_free_titledb(list_item* item) {
    if(item == NULL) {
        return;
    }

    if(item->data != NULL) {
        titledb_info* titledbInfo = (titledb_info*) item->data;
        if(titledbInfo->meta.texture != 0) {
            screen_unload_texture(titledbInfo->meta.texture);
            titledbInfo->meta.texture = 0;
        }

        free(item->data);
    }

    free(item);
}

void task_clear_titledb(linked_list* items) {
    if(items == NULL) {
        return;
    }

    linked_list_iter iter;
    linked_list_iterate(items, &iter);

    while(linked_list_iter_has_next(&iter)) {
        list_item* item = (list_item*) linked_list_iter_next(&iter);

        linked_list_iter_remove(&iter);
        task_free_titledb(item);
    }
}

Result task_populate_titledb(populate_titledb_data* data) {
    if(data == NULL || data->items == NULL) {
        return R_APP_INVALID_ARGUMENT;
    }

    task_clear_titledb(data->items);

    data->itemsListed = false;
    data->finished = false;
    data->result = 0;
    data->cancelEvent = 0;

    Result res = 0;
    if(R_SUCCEEDED(res = svcCreateEvent(&data->cancelEvent, RESET_STICKY))) {
        if(R_SUCCEEDED(res = svcCreateEvent(&data->resumeEvent, RESET_STICKY))) {
            svcSignalEvent(data->resumeEvent);

            if(threadCreate(task_populate_titledb_thread, data, 0x10000, 0x19, 1, true) == NULL) {
                res = R_APP_THREAD_CREATE_FAILED;
            }
        }
    }

    if(R_FAILED(res)) {
        data->itemsListed = true;
        data->finished = true;

        if(data->resumeEvent != 0) {
            svcCloseHandle(data->resumeEvent);
            data->resumeEvent = 0;
        }

        if(data->cancelEvent != 0) {
            svcCloseHandle(data->cancelEvent);
            data->cancelEvent = 0;
        }
    }

    return res;
}