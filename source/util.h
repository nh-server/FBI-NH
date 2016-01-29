#pragma once

void util_panic(const char* s, ...);

bool util_is_dir(FS_Archive* archive, const char* path);
Result util_traverse_contents(FS_Archive* archive, const char* path, bool recursive, bool dirsFirst, void* data, bool (*filter)(void* data, FS_Archive* archive, const char* path, u32 attributes),
                                                                                                                 void (*process)(void* data, FS_Archive* archive, const char* path, u32 attributes));
bool util_filter_dirs(void* data, FS_Archive* archive, const char* path, u32 attributes);
bool util_filter_files(void* data, FS_Archive* archive, const char* path, u32 attributes);
bool util_filter_hidden(void* data, FS_Archive* archive, const char* path, u32 attributes);
bool util_filter_file_extension(void* data, FS_Archive* archive, const char* path, u32 attributes);
bool util_filter_not_path(void* data, FS_Archive* archive, const char* path, u32 attributes);
Result util_count_contents(u32* out, FS_Archive* archive, const char* path, bool recursive, bool dirsFirst, void* data, bool (*filter)(void* data, FS_Archive* archive, const char* path, u32 attributes));
Result util_populate_contents(char*** contentsOut, u32* countOut, FS_Archive* archive, const char* path, bool recursive, bool dirsFirst, void* data, bool (*filter)(void* data, FS_Archive* archive, const char* path, u32 attributes));
void util_free_contents(char** contents, u32 count);
void util_get_path_file(char* out, const char* path, u32 size);
void util_get_parent_path(char* out, const char* path, u32 size);
Result util_ensure_dir(FS_Archive* archive, const char* path);