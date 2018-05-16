#include <sys/stat.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <3ds.h>
#include <jansson.h>

#include "uitask.h"
#include "listtitledb.h"
#include "../resources.h"
#include "../../core/core.h"
#include "../../core/task/dataop.h"

#define json_object_get_string(obj, name, def) (json_is_string(json_object_get(obj, name)) ? json_string_value(json_object_get(obj, name)) : def)
#define json_object_get_integer(obj, name, def) (json_is_integer(json_object_get(obj, name)) ? json_integer_value(json_object_get(obj, name)) : def)

#define TITLEDB_CACHE_DIR "sdmc:/fbi/"
#define TITLEDB_CACHE_FILE TITLEDB_CACHE_DIR "titledb_cache.json"

static json_t* installedApps = NULL;

static void task_populate_titledb_cache_load() {
    if(installedApps != NULL) {
        json_decref(installedApps);
    }

    json_error_t error;
    installedApps = json_load_file(TITLEDB_CACHE_FILE, 0, &error);

    if(!json_is_object(installedApps)) {
        if(installedApps != NULL) {
            json_decref(installedApps);
        }

        installedApps = json_object();
    }
}

static void task_populate_titledb_cache_save() {
    if(json_is_object(installedApps)) {
        mkdir(TITLEDB_CACHE_DIR, 755);

        json_dump_file(installedApps, TITLEDB_CACHE_FILE, 0);
    }
}

void task_populate_titledb_cache_unload() {
    if(json_is_object(installedApps)) {
        task_populate_titledb_cache_save();

        json_decref(installedApps);
        installedApps = NULL;
    }
}

static json_t* task_populate_titledb_cache_get_base(u32 id, bool cia, bool create) {
    if(!json_is_object(installedApps)) {
        task_populate_titledb_cache_load();
    }

    if(json_is_object(installedApps)) {
        char idString[16];
        itoa(id, idString, 10);

        json_t* cache = json_object_get(installedApps, idString);
        if(!json_is_object(cache)) {
            if(create) {
                cache = json_object();
                json_object_set(installedApps, idString, cache);
            } else {
                cache = NULL;
            }
        }

        if(json_is_object(cache)) {
            // Get old cache entry.
            const char* objIdKey = cia ? "cia_id" : "tdsx_id";
            json_t* objId = json_object_get(cache, objIdKey);

            const char* objKey = cia ? "cia" : "tdsx";
            json_t* obj = json_object_get(cache, objKey);
            if(!json_is_object(obj)) {
                // Force creation if old value to migrate exists.
                if(create || json_is_integer(objId)) {
                    obj = json_object();
                    json_object_set(cache, objKey, obj);
                } else {
                    obj = NULL;
                }
            }

            // Migrate old cache entry.
            if(json_is_integer(objId)) {
                json_object_set(obj, "id", json_integer(json_integer_value(objId)));
                json_object_del(cache, objIdKey);
            }

            return obj;
        }
    }

    return NULL;
}

static bool task_populate_titledb_cache_get(u32 id, bool cia, titledb_cache_entry* entry) {
    json_t* obj = task_populate_titledb_cache_get_base(id, cia, false);
    if(json_is_object(obj)) {
        json_t* idJson = json_object_get(obj, "id");
        if(json_is_integer(idJson)) {
            entry->id = (u32) json_integer_value(idJson);
            string_copy(entry->mtime, json_object_get_string(obj, "mtime", "Unknown"), sizeof(entry->mtime));
            string_copy(entry->version, json_object_get_string(obj, "version", "Unknown"), sizeof(entry->version));

            return true;
        }
    }

    return false;
}

void task_populate_titledb_cache_set(u32 id, bool cia, titledb_cache_entry* entry) {
    json_t* obj = task_populate_titledb_cache_get_base(id, cia, true);
    if(json_is_object(obj)) {
        json_object_set(obj, "id", json_integer(entry->id));
        json_object_set(obj, "mtime", json_string(entry->mtime));
        json_object_set(obj, "version", json_string(entry->version));

        task_populate_titledb_cache_save();
    }
}

void task_populate_titledb_update_status(list_item* item) {
    titledb_info* info = (titledb_info*) item->data;

    info->cia.installed = false;
    info->tdsx.installed = false;

    if(info->cia.exists) {
        AM_TitleEntry entry;
        info->cia.installed = R_SUCCEEDED(AM_GetTitleInfo(fs_get_title_destination(info->cia.titleId), 1, &info->cia.titleId, &entry))
                              && task_populate_titledb_cache_get(info->id, true, &info->cia.installedInfo);
    }

    if(info->tdsx.exists) {
        char path3dsx[FILE_PATH_MAX];
        fs_make_3dsx_path(path3dsx, info->meta.shortDescription, sizeof(path3dsx));

        FS_Path* fsPath = fs_make_path_utf8(path3dsx);
        if(fsPath != NULL) {
            Handle handle = 0;
            if(R_SUCCEEDED(FSUSER_OpenFileDirectly(&handle, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), *fsPath, FS_OPEN_READ, 0))) {
                FSFILE_Close(handle);

                info->tdsx.installed = task_populate_titledb_cache_get(info->id, false, &info->tdsx.installedInfo);
            }

            fs_free_path_utf8(fsPath);
        }
    }

    if((info->cia.installed && info->cia.installedInfo.id != info->cia.id) || (info->tdsx.installed && info->tdsx.installedInfo.id != info->tdsx.id)) {
        item->color = COLOR_TITLEDB_OUTDATED;
    } else if(info->cia.installed || info->tdsx.installed) {
        item->color = COLOR_TITLEDB_INSTALLED;
    } else {
        item->color = COLOR_TITLEDB_NOT_INSTALLED;
    }
}

static int task_populate_titledb_compare_dates(const char* date1, const char* date2, size_t size) {
    bool unk1 = strncmp(date1, "Unknown", size) == 0;
    bool unk2 = strncmp(date2, "Unknown", size) == 0;

    if(unk1 && !unk2) {
        return -1;
    } else if(!unk1 && unk2) {
        return 1;
    } else if(unk1 && unk2) {
        return 0;
    }

    return strncmp(date1, date2, size);
}

static void task_populate_titledb_thread(void* arg) {
    populate_titledb_data* data = (populate_titledb_data*) arg;

    Result res = 0;

    linked_list titles;
    linked_list_init(&titles);

    json_t* root = NULL;
    if(R_SUCCEEDED(res = http_download_json("https://api.titledb.com/v1/entry?nested=true"
                                                    "&only=id&only=name&only=author&only=headline&only=category"
                                                    "&only=cia.id&only=cia.mtime&only=cia.version&only=cia.size&only=cia.titleid"
                                                    "&only=tdsx.id&only=tdsx.mtime&only=tdsx.version&only=tdsx.size&only=tdsx.smdh.id",
                                            &root, 1024 * 1024))) {
        if(json_is_array(root)) {
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
                            string_copy(titledbInfo->category, json_object_get_string(entry, "category", "Unknown"), sizeof(titledbInfo->category));
                            string_copy(titledbInfo->meta.shortDescription, json_object_get_string(entry, "name", ""), sizeof(titledbInfo->meta.shortDescription));
                            string_copy(titledbInfo->meta.publisher, json_object_get_string(entry, "author", ""), sizeof(titledbInfo->meta.publisher));

                            json_t* headline = json_object_get(entry, "headline");
                            if(json_is_string(headline)) {
                                const char* val = json_string_value(headline);

                                if(json_string_length(headline) > sizeof(titledbInfo->headline) - 1) {
                                    snprintf(titledbInfo->headline, sizeof(titledbInfo->headline), "%.508s...", val);
                                } else {
                                    string_copy(titledbInfo->headline, val, sizeof(titledbInfo->headline));
                                }
                            } else {
                                titledbInfo->headline[0] = '\0';
                            }

                            json_t* cias = json_object_get(entry, "cia");
                            if(json_is_array(cias)) {
                                for(u32 j = 0; j < json_array_size(cias); j++) {
                                    json_t* cia = json_array_get(cias, j);
                                    if(json_is_object(cia)) {
                                        const char* mtime = json_object_get_string(cia, "mtime", "Unknown");
                                        if(!titledbInfo->cia.exists || task_populate_titledb_compare_dates(mtime, titledbInfo->cia.mtime, sizeof(titledbInfo->cia.mtime)) >= 0) {
                                            titledbInfo->cia.exists = true;

                                            titledbInfo->cia.id = (u32) json_object_get_integer(cia, "id", 0);
                                            string_copy(titledbInfo->cia.mtime, mtime, sizeof(titledbInfo->cia.mtime));
                                            string_copy(titledbInfo->cia.version, json_object_get_string(cia, "version", "Unknown"), sizeof(titledbInfo->cia.version));
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
                                        const char* mtime = json_object_get_string(tdsx, "mtime", "Unknown");
                                        if(!titledbInfo->tdsx.exists || task_populate_titledb_compare_dates(mtime, titledbInfo->tdsx.mtime, sizeof(titledbInfo->tdsx.mtime)) >= 0) {
                                            titledbInfo->tdsx.exists = true;

                                            titledbInfo->tdsx.id = (u32) json_object_get_integer(tdsx, "id", 0);
                                            string_copy(titledbInfo->tdsx.mtime, mtime, sizeof(titledbInfo->tdsx.mtime));
                                            string_copy(titledbInfo->tdsx.version, json_object_get_string(tdsx, "version", "Unknown"), sizeof(titledbInfo->tdsx.version));
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

                            char* latestTime = "Unknown";
                            if(titledbInfo->cia.exists && titledbInfo->tdsx.exists) {
                                if(task_populate_titledb_compare_dates(titledbInfo->cia.mtime, titledbInfo->tdsx.mtime, sizeof(titledbInfo->cia.mtime)) >= 0) {
                                    latestTime = titledbInfo->cia.mtime;
                                } else {
                                    latestTime = titledbInfo->tdsx.mtime;
                                }
                            } else if(titledbInfo->cia.exists) {
                                latestTime = titledbInfo->cia.mtime;
                            } else if(titledbInfo->tdsx.exists) {
                                latestTime = titledbInfo->tdsx.mtime;
                            }

                            string_copy(titledbInfo->mtime, latestTime, sizeof(titledbInfo->mtime));

                            if((titledbInfo->cia.exists || titledbInfo->tdsx.exists) && (data->filter == NULL || data->filter(data->userData, titledbInfo))) {
                                string_copy(item->name, titledbInfo->meta.shortDescription, LIST_ITEM_NAME_MAX);
                                item->data = titledbInfo;

                                task_populate_titledb_update_status(item);

                                linked_list_add_sorted(&titles, item, data->userData, data->compare);
                            } else {
                                free(titledbInfo);
                                free(item);
                            }
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
                    linked_list_iter_remove(&iter);
                }
            }
        } else {
            res = R_APP_BAD_DATA;
        }

        json_decref(root);
    }

    data->itemsListed = true;

    if(R_SUCCEEDED(res)) {
        linked_list_iter iter;
        linked_list_iterate(&titles, &iter);

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
            if(R_SUCCEEDED(http_download(url, &iconSize, &icon, sizeof(icon))) && iconSize == sizeof(icon)) {
                titledbInfo->meta.texture = screen_allocate_free_texture();
                screen_load_texture_tiled(titledbInfo->meta.texture, icon, sizeof(icon), 48, 48, GPU_RGB565, false);
            }
        }
    }

    linked_list_destroy(&titles);

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