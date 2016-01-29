#include <3ds.h>

#include "action.h"
#include "../section.h"

void action_browse_title_save_data(title_info* info) {
    u32 path[3] = {info->mediaType, (u32) (info->titleId & 0xFFFFFFFF), (u32) ((info->titleId >> 32) & 0xFFFFFFFF)};
    FS_Archive archive = {ARCHIVE_USER_SAVEDATA, {PATH_BINARY, 12, path}};
    files_open(archive);
}