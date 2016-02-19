#pragma once

typedef struct {
    u16 shortDescription[0x40];
    u16 longDescription[0x80];
    u16 publisher[0x40];
} SMDH_title;

typedef struct {
    char magic[0x04];
    u16 version;
    u16 reserved1;
    SMDH_title titles[0x10];
    u8 ratings[0x10];
    u32 region;
    u32 matchMakerId;
    u64 matchMakerBitId;
    u32 flags;
    u16 eulaVersion;
    u16 reserved;
    u32 optimalBannerFrame;
    u32 streetpassId;
    u64 reserved2;
    u8 smallIcon[0x480];
    u8 largeIcon[0x1200];
} SMDH;

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

int util_compare_u64(const void* e1, const void* e2);
int util_compare_directory_entries(const void* e1, const void* e2);