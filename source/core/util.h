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

typedef struct {
    u8 version;
    bool animated;
    u16 crc16[4];
    u8 reserved[0x16];
    u8 mainIconBitmap[0x200];
    u16 mainIconPalette[0x10];
    u16 titles[16][0x80];
    u8 animatedFrameBitmaps[8][0x200];
    u16 animatedFramePalettes[8][0x10];
    u16 animationSequence[0x40];
} BNR;

void util_panic(const char* s, ...);

FS_Path* util_make_path_utf8(const char* path);
void util_free_path_utf8(FS_Path* path);

FS_Path util_make_binary_path(const void* data, u32 size);

bool util_is_dir(FS_Archive archive, const char* path);
Result util_ensure_dir(FS_Archive archive, const char* path);

void util_get_path_file(char* out, const char* path, u32 size);
void util_get_parent_path(char* out, const char* path, u32 size);