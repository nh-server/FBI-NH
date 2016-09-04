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

static Result task_populate_titledb_download(u32* downloadSize, void* buffer, u32 maxSize, const char* url) {
    Result res = 0;

    if(downloadSize != NULL) {
        *downloadSize = 0;
    }

    httpcContext context;
    if(R_SUCCEEDED(res = httpcOpenContext(&context, HTTPC_METHOD_GET, url, 1))) {
        char userAgent[128];
        snprintf(userAgent, sizeof(userAgent), "Mozilla/5.0 (Nintendo 3DS; Mobile; rv:10.0) Gecko/20100101 FBI/%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO);

        u32 responseCode = 0;
        if(R_SUCCEEDED(res = httpcSetSSLOpt(&context, SSLCOPT_DisableVerify))
           && R_SUCCEEDED(res = httpcAddRequestHeaderField(&context, "User-Agent", userAgent))
           && R_SUCCEEDED(res = httpcAddRequestHeaderField(&context, "Connection", "Keep-Alive"))
           && R_SUCCEEDED(res = httpcBeginRequest(&context))
           && R_SUCCEEDED(res = httpcGetResponseStatusCode(&context, &responseCode))) {
            if(responseCode == 200) {
                u32 size = 0;
                u32 bytesRead = 0;
                while(size < maxSize && (res = httpcDownloadData(&context, &((u8*) buffer)[size], maxSize - size < 0x1000 ? maxSize - size : 0x1000, &bytesRead)) == HTTPC_RESULTCODE_DOWNLOADPENDING) {
                    size += bytesRead;
                }

                size += bytesRead;

                if(R_SUCCEEDED(res) && downloadSize != NULL) {
                    *downloadSize = size;
                }
            } else {
                res = R_FBI_HTTP_RESPONSE_CODE;
            }
        }

        httpcCloseContext(&context);
    }

    return res;
}

static int task_populate_titledb_compare(const void** p1, const void** p2) {
    list_item* info1 = *(list_item**) p1;
    list_item* info2 = *(list_item**) p2;

    return strncasecmp(info1->name, info2->name, LIST_ITEM_NAME_MAX);
}

static void task_populate_titledb_thread(void* arg) {
    populate_titledb_data* data = (populate_titledb_data*) arg;

    Result res = 0;

    linked_list tempItems;
    linked_list_init(&tempItems);

    u32 maxTextSize = 128 * 1024;
    char* text = (char*) calloc(sizeof(char), maxTextSize);
    if(text != NULL) {
        u32 textSize = 0;
        if(R_SUCCEEDED(res = task_populate_titledb_download(&textSize, text, maxTextSize, "https://api.titledb.com/v0/"))) {
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
                                            } else if(strncmp(name, "name", nameLen) == 0) {
                                                strncpy(titledbInfo->meta.shortDescription, subVal->u.string.ptr, sizeof(titledbInfo->meta.shortDescription));
                                            } else if(strncmp(name, "description", nameLen) == 0) {
                                                strncpy(titledbInfo->meta.longDescription, subVal->u.string.ptr, sizeof(titledbInfo->meta.longDescription));
                                            } else if(strncmp(name, "author", nameLen) == 0) {
                                                strncpy(titledbInfo->meta.publisher, subVal->u.string.ptr, sizeof(titledbInfo->meta.publisher));
                                            }
                                        } else if(subVal->type == json_integer) {
                                            if(strncmp(name, "size", nameLen) == 0) {
                                                titledbInfo->size = (u64) subVal->u.integer;
                                            }
                                        }
                                    }

                                    if(strlen(titledbInfo->meta.shortDescription) > 0) {
                                        strncpy(item->name, titledbInfo->meta.shortDescription, LIST_ITEM_NAME_MAX);
                                    } else {
                                        snprintf(item->name, LIST_ITEM_NAME_MAX, "%016llX", titledbInfo->titleId);
                                    }

                                    AM_TitleEntry entry;
                                    if(R_SUCCEEDED(AM_GetTitleInfo(util_get_title_destination(titledbInfo->titleId), 1, &titledbInfo->titleId, &entry))) {
                                        item->color = COLOR_INSTALLED;
                                    } else {
                                        item->color = COLOR_NOT_INSTALLED;
                                    }

                                    item->data = titledbInfo;

                                    linked_list_add(&tempItems, item);
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
        linked_list_sort(&tempItems, task_populate_titledb_compare);

        linked_list_iter tempIter;
        linked_list_iterate(&tempItems, &tempIter);

        while(linked_list_iter_has_next(&tempIter)) {
            linked_list_add(data->items, linked_list_iter_next(&tempIter));
        }

        linked_list_iter iter;
        linked_list_iterate(data->items, &iter);

        while(linked_list_iter_has_next(&iter)) {
            svcWaitSynchronization(task_get_pause_event(), U64_MAX);
            if(task_is_quit_all() || svcWaitSynchronization(data->cancelEvent, 0) == 0) {
                break;
            }

            list_item* item = (list_item*) linked_list_iter_next(&iter);
            titledb_info* titledbInfo = (titledb_info*) item->data;

            u32 maxPngSize = 128 * 1024;
            u8* png = (u8*) calloc(1, maxPngSize);
            if(png != NULL) {
                char pngUrl[128];
                snprintf(pngUrl, sizeof(pngUrl), "https://api.titledb.com/images/%016llX.png", titledbInfo->titleId);

                u32 pngSize = 0;
                if(R_SUCCEEDED(task_populate_titledb_download(&pngSize, png, maxPngSize, pngUrl))) {
                    int width;
                    int height;
                    int depth;
                    u8* image = stbi_load_from_memory(png, (int) pngSize, &width, &height, &depth, STBI_rgb_alpha);
                    if(image != NULL && depth == STBI_rgb_alpha) {
                        for(u32 x = 0; x < width; x++) {
                            for(u32 y = 0; y < height; y++) {
                                u32 pos = (y * width + x) * 4;

                                u8 c1 = image[pos + 0];
                                u8 c2 = image[pos + 1];
                                u8 c3 = image[pos + 2];
                                u8 c4 = image[pos + 3];

                                image[pos + 0] = c4;
                                image[pos + 1] = c3;
                                image[pos + 2] = c2;
                                image[pos + 3] = c1;
                            }
                        }

                        titledbInfo->meta.texture = screen_load_texture_auto(image, (u32) (width * height * 4), (u32) width, (u32) height, GPU_RGBA8, false);

                        free(image);
                    }
                }

                free(png);
            }
        }
    }

    linked_list_destroy(&tempItems);

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