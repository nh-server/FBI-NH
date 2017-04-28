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
#include "../../../json/json.h"
#include "../../../stb_image/stb_image.h"

void task_populate_titledb_update_status(list_item* item) {
    titledb_info* info = (titledb_info*) item->data;

    AM_TitleEntry entry;
    info->installed = R_SUCCEEDED(AM_GetTitleInfo(util_get_title_destination(info->titleId), 1, &info->titleId, &entry));
    info->installedVersion = info->installed ? entry.version : (u16) 0;

    if(info->installed) {
        if(info->installedVersion < info->latestVersion) {
            item->color = COLOR_TITLEDB_OUTDATED;
        } else {
            item->color = COLOR_TITLEDB_INSTALLED;
        }
    } else {
        item->color = COLOR_TITLEDB_NOT_INSTALLED;
    }
}

static Result task_populate_titledb_download(u32* downloadSize, void* buffer, u32 maxSize, const char* url) {
    Result res = 0;

    httpcContext context;
    if(R_SUCCEEDED(res = util_http_open(&context, NULL, url, true))) {
        res = util_http_read(&context, downloadSize, buffer, maxSize);

        Result closeRes = util_http_close(&context);
        if(R_SUCCEEDED(res)) {
            res = closeRes;
        }
    }

    return res;
}

static int task_populate_titledb_compare(void* userData, const void* p1, const void* p2) {
    list_item* info1 = (list_item*) p1;
    list_item* info2 = (list_item*) p2;

    return strncasecmp(info1->name, info2->name, LIST_ITEM_NAME_MAX);
}

static void task_populate_titledb_thread(void* arg) {
    populate_titledb_data* data = (populate_titledb_data*) arg;

    Result res = 0;

    u32 maxTextSize = 256 * 1024;
    char* text = (char*) calloc(sizeof(char), maxTextSize);
    if(text != NULL) {
        u32 textSize = 0;
        if(R_SUCCEEDED(res = task_populate_titledb_download(&textSize, text, maxTextSize, "https://api.titledb.com/v1/cia?only=id&only=size&only=titleid&only=version&only=name_s&only=name_l&only=publisher"))) {
            json_value* json = json_parse(text, textSize);
            if(json != NULL) {
                if(json->type == json_array) {
                    for(u32 i = 0; i < json->u.array.length && R_SUCCEEDED(res); i++) {
                        svcWaitSynchronization(task_get_pause_event(), U64_MAX);
                        if(task_is_quit_all() || svcWaitSynchronization(data->cancelEvent, 0) == 0) {
                            break;
                        }

                        json_value* val = json->u.array.values[i];
                        if(val->type == json_object) {
                            list_item* item = (list_item*) calloc(1, sizeof(list_item));
                            if(item != NULL) {
                                titledb_info* titledbInfo = (titledb_info*) calloc(1, sizeof(titledb_info));
                                if(titledbInfo != NULL) {
                                    for(u32 j = 0; j < val->u.object.length; j++) {
                                        char* name = val->u.object.values[j].name;
                                        u32 nameLen = val->u.object.values[j].name_length;
                                        json_value* subVal = val->u.object.values[j].value;
                                        if(subVal->type == json_string) {
                                            if(strncmp(name, "titleid", nameLen) == 0) {
                                                titledbInfo->titleId = strtoull(subVal->u.string.ptr, NULL, 16);
                                            } else if(strncmp(name, "version", nameLen) == 0) {
                                                u32 major = 0;
                                                u32 minor = 0;
                                                u32 micro = 0;
                                                sscanf(subVal->u.string.ptr, "%lu.%lu.%lu", &major, &minor, &micro);

                                                titledbInfo->latestVersion = ((u8) (major & 0x3F) << 10) | ((u8) (minor & 0x3F) << 4) | ((u8) (micro & 0xF));
                                            } else if(strncmp(name, "name_s", nameLen) == 0) {
                                                strncpy(titledbInfo->meta.shortDescription, subVal->u.string.ptr, sizeof(titledbInfo->meta.shortDescription));
                                            } else if(strncmp(name, "name_l", nameLen) == 0) {
                                                strncpy(titledbInfo->meta.longDescription, subVal->u.string.ptr, sizeof(titledbInfo->meta.longDescription));
                                            } else if(strncmp(name, "publisher", nameLen) == 0) {
                                                strncpy(titledbInfo->meta.publisher, subVal->u.string.ptr, sizeof(titledbInfo->meta.publisher));
                                            }
                                        } else if(subVal->type == json_integer) {
                                            if(strncmp(name, "id", nameLen) == 0) {
                                                titledbInfo->id = (u32) subVal->u.integer;
                                            } else if(strncmp(name, "size", nameLen) == 0) {
                                                titledbInfo->size = (u64) subVal->u.integer;
                                            }
                                        }
                                    }

                                    if(strlen(titledbInfo->meta.shortDescription) > 0) {
                                        strncpy(item->name, titledbInfo->meta.shortDescription, LIST_ITEM_NAME_MAX);
                                    } else {
                                        snprintf(item->name, LIST_ITEM_NAME_MAX, "%016llX", titledbInfo->titleId);
                                    }

                                    item->data = titledbInfo;

                                    task_populate_titledb_update_status(item);

                                    linked_list_iter iter;
                                    linked_list_iterate(data->items, &iter);

                                    bool add = true;
                                    while(linked_list_iter_has_next(&iter)) {
                                        list_item* currItem = (list_item*) linked_list_iter_next(&iter);
                                        titledb_info* currTitledbInfo = (titledb_info*) currItem->data;

                                        if(titledbInfo->titleId == currTitledbInfo->titleId) {
                                            if(titledbInfo->latestVersion >= currTitledbInfo->latestVersion) {
                                                linked_list_iter_remove(&iter);
                                                task_free_titledb(currItem);
                                            } else {
                                                add = false;
                                            }

                                            break;
                                        }
                                    }

                                    if(add) {
                                        linked_list_add_sorted(data->items, item, NULL, task_populate_titledb_compare);
                                    } else {
                                        task_free_titledb(item);
                                    }
                                } else {
                                    free(item);

                                    res = R_FBI_OUT_OF_MEMORY;
                                }
                            } else {
                                res = R_FBI_OUT_OF_MEMORY;
                            }
                        }
                    }
                } else {
                    res = R_FBI_BAD_DATA;
                }
            } else {
                res = R_FBI_PARSE_FAILED;
            }
        }

        free(text);
    } else {
        res = R_FBI_OUT_OF_MEMORY;
    }

    if(R_SUCCEEDED(res)) {
        linked_list_iter iter;
        linked_list_iterate(data->items, &iter);

        while(linked_list_iter_has_next(&iter)) {
            svcWaitSynchronization(task_get_pause_event(), U64_MAX);
            if(task_is_quit_all() || svcWaitSynchronization(data->cancelEvent, 0) == 0) {
                break;
            }

            list_item* item = (list_item*) linked_list_iter_next(&iter);
            titledb_info* titledbInfo = (titledb_info*) item->data;

            char url[128];
            snprintf(url, sizeof(url), "https://3ds.titledb.com/v1/cia/%lu/icon_l.bin", titledbInfo->id);

            u8 icon[0x1200];
            u32 iconSize = 0;
            if(R_SUCCEEDED(task_populate_titledb_download(&iconSize, &icon, sizeof(icon), url)) && iconSize == sizeof(icon)) {
                titledbInfo->meta.texture = screen_allocate_free_texture();
                screen_load_texture_tiled(titledbInfo->meta.texture, icon, sizeof(icon), 48, 48, GPU_RGB565, false);
            }
        }
    }

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
        return R_FBI_INVALID_ARGUMENT;
    }

    task_clear_titledb(data->items);

    data->finished = false;
    data->result = 0;
    data->cancelEvent = 0;

    Result res = 0;
    if(R_SUCCEEDED(res = svcCreateEvent(&data->cancelEvent, RESET_STICKY))) {
        if(threadCreate(task_populate_titledb_thread, data, 0x10000, 0x19, 1, true) == NULL) {
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