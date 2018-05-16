#include <string.h>

#include <3ds.h>

#include "action.h"
#include "../task/uitask.h"
#include "../../core/core.h"

void action_mark_titledb_updated(linked_list* items, list_item* selected, bool cia) {
    titledb_info* info = (titledb_info*) selected->data;

    if((cia && info->cia.installed) || (!cia && info->tdsx.installed)) {
        titledb_cache_entry entry;
        if(cia) {
            entry.id = info->cia.id;
            string_copy(entry.mtime, info->cia.mtime, sizeof(entry.mtime));
            string_copy(entry.version, info->cia.version, sizeof(entry.version));
        } else {
            entry.id = info->tdsx.id;
            string_copy(entry.mtime, info->tdsx.mtime, sizeof(entry.mtime));
            string_copy(entry.version, info->tdsx.version, sizeof(entry.version));
        }

        task_populate_titledb_cache_set(info->id, cia, &entry);
        task_populate_titledb_update_status(selected);
    }
}