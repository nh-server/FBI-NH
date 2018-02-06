#pragma once

typedef struct json_t json_t;

// File constants
#define FILE_NAME_MAX 512
#define FILE_PATH_MAX 512

// Strings
bool util_is_string_empty(const char* str);

// Paths
const char* util_get_3dsx_path();
void util_set_3dsx_path(const char* path);

void util_get_file_name(char* out, const char* file, u32 size);
void util_escape_file_name(char* out, const char* file, size_t size);
void util_get_path_file(char* out, const char* path, u32 size);
void util_get_parent_path(char* out, const char* path, u32 size);

bool util_filter_cias(void* data, const char* name, u32 attributes);
bool util_filter_tickets(void* data, const char* name, u32 attributes);

// Files
Result util_open_archive(FS_Archive* archive, FS_ArchiveID id, FS_Path path);
Result util_ref_archive(FS_Archive archive);
Result util_close_archive(FS_Archive archive);

FS_Path util_make_binary_path(const void* data, u32 size);
FS_Path* util_make_path_utf8(const char* path);
void util_free_path_utf8(FS_Path* path);

bool util_is_dir(FS_Archive archive, const char* path);
Result util_ensure_dir(FS_Archive archive, const char* path);

// Titles
FS_MediaType util_get_title_destination(u64 titleId);