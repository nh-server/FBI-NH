#include <stdio.h>
#include <stdlib.h>

#include <3ds.h>

#include "action.h"
#include "../task/task.h"
#include "../../error.h"
#include "../../list.h"
#include "../../ui.h"
#include "../../../core/linkedlist.h"

static void action_update_titledb_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2, u32 index) {
    linked_list* items = (linked_list*) data;

    if(index < linked_list_size(items)) {
        ui_draw_titledb_info(view, ((list_item*) linked_list_get(items, index))->data, x1, y1, x2, y2);
    }
}

void action_update_titledb(linked_list* items, list_item* selected) {
    char* urls = (char*) calloc(1, INSTALL_URL_MAX * INSTALL_URLS_MAX);
    if(urls != NULL) {
        linked_list_iter iter;
        linked_list_iterate(items, &iter);

        size_t pos = 0;
        while(linked_list_iter_has_next(&iter) && pos < INSTALL_URL_MAX * INSTALL_URLS_MAX) {
            titledb_info* info = (titledb_info*) ((list_item*) linked_list_iter_next(&iter))->data;

            if(info->installed && info->installedVersion < info->latestVersion) {
                pos += snprintf(urls + pos, (INSTALL_URL_MAX * INSTALL_URLS_MAX) - pos, "https://3ds.titledb.com/v1/cia/%lu/download\n", info->id);
            }
        }

        action_url_install("Update installed titles from TitleDB?", urls, items, NULL, action_update_titledb_draw_top);

        free(urls);
    } else {
        error_display_res(NULL, NULL, R_FBI_OUT_OF_MEMORY, "Failed to allocate URL text buffer.");
    }
}