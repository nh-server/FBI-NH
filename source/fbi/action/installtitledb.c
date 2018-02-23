#include <stdio.h>
#include <stdlib.h>

#include <3ds.h>

#include "action.h"
#include "../task/uitask.h"
#include "../../core/core.h"

typedef struct {
    list_item* selected;
    bool cia;
} install_titledb_data;

static void action_install_titledb_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, u32 index) {
    install_titledb_data* installData = (install_titledb_data*) data;

    if(installData->cia) {
        task_draw_titledb_info_cia(view, installData->selected->data, x1, y1, x2, y2);
    } else {
        task_draw_titledb_info_tdsx(view, installData->selected->data, x1, y1, x2, y2);
    }
}

static void action_install_titledb_finished_url(void* data, u32 index) {
    install_titledb_data* installData = (install_titledb_data*) data;
    list_item* item = installData->selected;
    titledb_info* info = (titledb_info*) item->data;

    task_populate_titledb_cache_installed(info->id, installData->cia ? info->cia.id : info->tdsx.id, installData->cia);
    task_populate_titledb_update_status(item);
}

static void action_install_titledb_finished_all(void* data) {
    free(data);
}

void action_install_titledb(linked_list* items, list_item* selected, bool cia) {
    install_titledb_data* data = (install_titledb_data*) calloc(1, sizeof(install_titledb_data));
    if(data == NULL) {
        error_display(NULL, NULL, "Failed to allocate install TitleDB data.");

        return;
    }

    data->selected = selected;
    data->cia = cia;

    titledb_info* info = (titledb_info*) selected->data;

    char urls[2 * DOWNLOAD_URL_MAX];
    char paths[2 * FILE_PATH_MAX];
    if(data->cia) {
        snprintf(urls, sizeof(urls), "https://3ds.titledb.com/v1/cia/%lu/download", info->cia.id);
    } else {
        char name[FILE_NAME_MAX];
        string_escape_file_name(name, info->meta.shortDescription, sizeof(name));

        u32 urlsPos = 0;
        u32 pathsPos = 0;

        urlsPos += snprintf(urls + urlsPos, sizeof(urls) - urlsPos, "https://3ds.titledb.com/v1/tdsx/%lu/download\n", info->tdsx.id);
        pathsPos += snprintf(paths + pathsPos, sizeof(paths) - pathsPos, "/3ds/%s/%s.3dsx\n", name, name);

        if(info->tdsx.smdh.exists) {
            snprintf(urls + urlsPos, sizeof(urls) - urlsPos, "https://3ds.titledb.com/v1/smdh/%lu/download\n", info->tdsx.smdh.id);
            snprintf(paths + pathsPos, sizeof(paths) - pathsPos, "/3ds/%s/%s.smdh\n", name, name);
        }
    }

    action_install_url("Install the selected title from TitleDB?", urls, paths, data, action_install_titledb_finished_url, action_install_titledb_finished_all, action_install_titledb_draw_top);
}