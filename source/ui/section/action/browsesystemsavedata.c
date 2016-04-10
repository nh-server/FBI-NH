#include <3ds.h>

#include "action.h"
#include "../section.h"

void action_browse_system_save_data(system_save_data_info* info, bool* populated) {
    u32 path[2] = {MEDIATYPE_NAND, info->systemSaveDataId};
    FS_Archive archive = {ARCHIVE_SYSTEM_SAVEDATA, {PATH_BINARY, 8, path}};
    files_open(archive);
}