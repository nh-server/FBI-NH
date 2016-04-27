#include <3ds.h>

#include "action.h"
#include "../section.h"
#include "../task/task.h"
#include "../../list.h"

void action_browse_system_save_data(linked_list* items, list_item* selected) {
    system_save_data_info* info = (system_save_data_info*) selected->data;

    u32 path[2] = {MEDIATYPE_NAND, info->systemSaveDataId};
    FS_Archive archive = {ARCHIVE_SYSTEM_SAVEDATA, {PATH_BINARY, 8, path}};
    files_open(archive);
}