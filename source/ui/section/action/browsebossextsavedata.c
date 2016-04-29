#include <3ds.h>

#include "action.h"
#include "../section.h"
#include "../task/task.h"
#include "../../list.h"
#include "../../../core/util.h"

void action_browse_boss_ext_save_data(linked_list* items, list_item* selected) {
    ext_save_data_info* info = (ext_save_data_info*) selected->data;

    u32 path[3] = {info->mediaType, (u32) (info->extSaveDataId & 0xFFFFFFFF), (u32) ((info->extSaveDataId >> 32) & 0xFFFFFFFF)};
    files_open(ARCHIVE_BOSS_EXTDATA, util_make_binary_path(path, sizeof(path)));
}