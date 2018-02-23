#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <3ds.h>

#include "action.h"
#include "../task/uitask.h"
#include "../../core/core.h"

typedef struct {
    char urls[INSTALL_URLS_MAX * DOWNLOAD_URL_MAX];
    char paths[INSTALL_URLS_MAX * FILE_PATH_MAX];

    bool cia[INSTALL_URLS_MAX];
    list_item* items[INSTALL_URLS_MAX];
} update_titledb_data;

static void action_update_titledb_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, u32 index) {
    update_titledb_data* updateData = (update_titledb_data*) data;

    if(updateData->items[index] != NULL) {
        if(updateData->cia[index]) {
            task_draw_titledb_info_cia(view, updateData->items[index]->data, x1, y1, x2, y2);
        } else {
            task_draw_titledb_info_tdsx(view, updateData->items[index]->data, x1, y1, x2, y2);
        }
    }
}

static void action_update_titledb_finished_url(void* data, u32 index) {
    update_titledb_data* updateData = (update_titledb_data*) data;
    list_item* item = updateData->items[index];
    titledb_info* info = (titledb_info*) item->data;

    task_populate_titledb_cache_installed(info->id, updateData->cia[index] ? info->cia.id : info->tdsx.id, updateData->cia[index]);
    task_populate_titledb_update_status(item);
}

static void action_update_titledb_finished_all(void* data) {
    free(data);
}

void action_update_titledb(linked_list* items, list_item* selected) {
    update_titledb_data* data = (update_titledb_data*) calloc(1, sizeof(update_titledb_data));
    if(data == NULL) {
        error_display(NULL, NULL, "Failed to allocate install TitleDB data.");

        return;
    }

    linked_list_iter iter;
    linked_list_iterate(items, &iter);

    u32 index = 0;
    u32 urlsPos = 0;
    u32 pathsPos = 0;
    while(linked_list_iter_has_next(&iter) && index < INSTALL_URLS_MAX && urlsPos < INSTALL_URLS_MAX * DOWNLOAD_URL_MAX && pathsPos < INSTALL_URLS_MAX * FILE_PATH_MAX) {
        list_item* item = linked_list_iter_next(&iter);
        titledb_info* info = (titledb_info*) item->data;

        if(info->cia.outdated) {
            urlsPos += snprintf(data->urls + urlsPos, INSTALL_URLS_MAX * DOWNLOAD_URL_MAX - urlsPos,
                                "https://3ds.titledb.com/v1/cia/%lu/download\n",
                                info->cia.id);
            pathsPos += snprintf(data->paths + pathsPos, INSTALL_URLS_MAX * FILE_PATH_MAX - pathsPos,
                                 "\n");

            data->cia[index] = true;
            data->items[index] = item;

            index++;
        }

        if(info->tdsx.outdated && (!info->tdsx.smdh.exists || index < INSTALL_URLS_MAX - 1)) {
            char name[FILE_NAME_MAX];
            string_escape_file_name(name, info->meta.shortDescription, sizeof(name));

            urlsPos += snprintf(data->urls + urlsPos, INSTALL_URLS_MAX * DOWNLOAD_URL_MAX - urlsPos, "https://3ds.titledb.com/v1/tdsx/%lu/download\n", info->tdsx.id);
            pathsPos += snprintf(data->paths + pathsPos, INSTALL_URLS_MAX * FILE_PATH_MAX - pathsPos, "/3ds/%s/%s.3dsx\n", name, name);
            data->cia[index] = false;
            data->items[index] = item;

            index++;

            if(info->tdsx.smdh.exists) {
                urlsPos += snprintf(data->urls + urlsPos, INSTALL_URLS_MAX * DOWNLOAD_URL_MAX - urlsPos, "https://3ds.titledb.com/v1/smdh/%lu/download\n", info->tdsx.smdh.id);
                pathsPos += snprintf(data->paths + pathsPos, INSTALL_URLS_MAX * FILE_PATH_MAX - pathsPos, "/3ds/%s/%s.smdh\n", name, name);
                data->cia[index] = false;
                data->items[index] = item;

                index++;
            }
        }
    }

    if(index > 0) {
        action_install_url("Install all updates from TitleDB?", data->urls, data->paths, data, action_update_titledb_finished_url, action_update_titledb_finished_all, action_update_titledb_draw_top);
    } else {
        prompt_display_notify("Success", "All titles are up to date.", COLOR_TEXT, NULL, NULL, NULL);
    }
}