#include <3ds.h>

#include "action.h"
#include "../section.h"
#include "../task/task.h"
#include "../../list.h"

void action_browse_title_save_data(linked_list* items, list_item* selected) {
    title_info* info = (title_info*) selected->data;

    u32 path[3] = {info->mediaType, (u32) (info->titleId & 0xFFFFFFFF), (u32) ((info->titleId >> 32) & 0xFFFFFFFF)};
    FS_Archive archive = {ARCHIVE_USER_SAVEDATA, {PATH_BINARY, 12, path}};
    files_open(archive);
}