#include <3ds.h>

#include "action.h"
#include "../section.h"
#include "../task/uitask.h"
#include "../../core/core.h"

void action_browse_system_save_data(linked_list* items, list_item* selected) {
    system_save_data_info* info = (system_save_data_info*) selected->data;

    u32 path[2] = {MEDIATYPE_NAND, info->systemSaveDataId};
    files_open(ARCHIVE_SYSTEM_SAVEDATA, fs_make_path_binary(path, sizeof(path)));
}