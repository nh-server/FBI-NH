#include <stdio.h>

#include <3ds.h>

#include "action.h"
#include "../task/task.h"
#include "../../list.h"

void action_install_titledb(linked_list* items, list_item* selected) {
    char url[128];
    snprintf(url, sizeof(url), "https://api.titledb.com/v0/proxy/%016llX", ((titledb_info*) selected->data)->titleId);

    action_url_install("Install the selected title from TitleDB?", url, NULL, NULL);
}