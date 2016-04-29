#pragma once

#include <stdbool.h>

typedef struct file_info_s file_info;

bool clipboard_has_contents();
FS_Archive clipboard_get_archive();
char* clipboard_get_path();
bool clipboard_is_contents_only();
Result clipboard_set_contents(FS_ArchiveID archiveId, FS_Path* archivePath, const char* path, bool contentsOnly);
void clipboard_clear();