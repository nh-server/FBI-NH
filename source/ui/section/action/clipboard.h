#pragma once

#include <stdbool.h>

bool clipboard_has_contents();
FS_Archive* clipboard_get_archive();
char* clipboard_get_name();
char* clipboard_get_path();
bool clipboard_is_cut();
Result clipboard_set_contents(FS_Archive archive, const char* path);
void clipboard_clear();