#include <stdio.h>
#include <stdlib.h>

#include <3ds.h>

#include "action.h"
#include "../task/task.h"
#include "../../error.h"
#include "../../list.h"
#include "../../../core/linkedlist.h"

void action_install_titledb(linked_list* items, list_item* selected) {
    char url[64];
    snprintf(url, INSTALL_URL_MAX, "https://3ds.titledb.com/v1/cia/%lu/download", ((titledb_info*) selected->data)->id);

    action_url_install("Install the selected title from TitleDB?", url, NULL, NULL);
}

void action_update_titledb(linked_list* items, list_item* selected) {
    char* urls = (char*) calloc(1, INSTALL_URL_MAX * INSTALL_URLS_MAX);
    if(urls != NULL) {
        linked_list_iter iter;
        linked_list_iterate(items, &iter);

        size_t pos = 0;
        while(linked_list_iter_has_next(&iter) && pos < INSTALL_URL_MAX * INSTALL_URLS_MAX) {
            titledb_info* info = (titledb_info*) ((list_item*) linked_list_iter_next(&iter))->data;

            if(info->outdated) {
                pos += snprintf(urls + pos, (INSTALL_URL_MAX * INSTALL_URLS_MAX) - pos, "https://3ds.titledb.com/v1/cia/%lu/download\n", info->id);
            }
        }

        action_url_install("Update installed titles from TitleDB?", urls, NULL, NULL);

        free(urls);
    } else {
        error_display_res(NULL, NULL, R_FBI_OUT_OF_MEMORY, "Failed to allocate URL text buffer.");
    }
}