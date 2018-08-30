#pragma once

#define FILE_NAME_MAX 256
#define FILE_PATH_MAX 512

typedef struct fs_filter_data_s {
    bool (*parentFilter)(void* data, const char* name, u32 attributes);
    void* parentFilterData;
} fs_filter_data;

bool fs_is_dir(FS_Archive archive, const char* path);
Result fs_ensure_dir(FS_Archive archive, const char* path);

FS_Path fs_make_path_binary(const void* data, u32 size);
FS_Path* fs_make_path_utf8(const char* path);
void fs_free_path_utf8(FS_Path* path);

Result fs_open_archive(FS_Archive* archive, FS_ArchiveID id, FS_Path path);
Result fs_ref_archive(FS_Archive archive);
Result fs_close_archive(FS_Archive archive);

const char* fs_get_3dsx_path();
void fs_set_3dsx_path(const char* path);

int fs_make_3dsx_path(char* out, const char* name, size_t size);
int fs_make_smdh_path(char* out, const char* name, size_t size);

FS_MediaType fs_get_title_destination(u64 titleId);

bool fs_filter_cias(void* data, const char* name, u32 attributes);
bool fs_filter_tickets(void* data, const char* name, u32 attributes);